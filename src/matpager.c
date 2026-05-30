#include "matpager.h"
#include "err.h"
#include "input.h"
#include "render.h"
#include "scan.h"
#include "term.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "paige.h"

/* A read-only source with a lazily-built logical-line offset index. */
struct src {
    const char *data;
    size_t size;
    bool mmapped;
    size_t *off; /* off[i] = byte offset of line i */
    size_t noff, off_cap;
    bool eof_known;
    size_t total; /* valid once eof_known */
};

static void off_push(struct src *s, size_t v)
{
    if (s->noff == s->off_cap) {
        s->off_cap = s->off_cap ? s->off_cap * 2 : 1024;
        s->off = realloc(s->off, s->off_cap * sizeof *s->off);
    }
    s->off[s->noff++] = v;
}

/* Ensure line offsets are known through index `want` (or to EOF). */
static void ensure(struct src *s, size_t want)
{
    while (!s->eof_known && s->noff <= want) {
        size_t from = s->off[s->noff - 1];
        if (from >= s->size) {
            s->eof_known = true;
            s->total = s->noff - 1; /* off[noff-1]==size: not a real line */
            return;
        }
        const unsigned char *base = (const unsigned char *)s->data;
        const unsigned char *q = mat_scan_newline(base + from, base + s->size);
        if (q == base + s->size) {
            s->eof_known = true;
            s->total =
                s->noff; /* final line [from, size) with no trailing nl */
            return;
        }
        size_t nl = (size_t)(q - base);
        if (nl + 1 < s->size) {
            off_push(s, nl + 1);
        } else {
            s->eof_known = true;
            s->total = s->noff; /* trailing newline: line `noff-1` ends here */
            return;
        }
    }
}

static bool src_line(struct src *s, size_t L, const unsigned char **d,
                     size_t *len)
{
    ensure(s, L + 1);
    if (s->eof_known && L >= s->total)
        return false;
    size_t start = s->off[L];
    size_t end = (L + 1 < s->noff) ? s->off[L + 1] - 1 : s->size;
    *d = (const unsigned char *)s->data + start;
    *len = end > start ? end - start : 0;
    return true;
}

static void src_free(struct src *s)
{
    if (s->mmapped && s->data && s->size)
        munmap((void *)s->data, s->size);
    else
        free((void *)s->data);
    free(s->off);
}

/* Read a non-seekable input (stdin/pipe) fully into memory. */
static char *slurp_fd(int fd, size_t *out)
{
    size_t cap = 1 << 16, len = 0;
    char *buf = malloc(cap);
    for (;;) {
        if (len == cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        ssize_t r = read(fd, buf + len, cap - len);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            free(buf);
            return NULL;
        }
        if (r == 0)
            break;
        len += (size_t)r;
    }
    *out = len;
    return buf;
}

static bool src_open(struct src *s, int fd)
{
    memset(s, 0, sizeof *s);
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
        void *m = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) {
            s->data = m;
            s->size = (size_t)st.st_size;
            s->mmapped = true;
        }
    }
    if (s->data == NULL) {
        s->data = slurp_fd(fd, &s->size);
        if (s->data == NULL)
            return false;
    }
    off_push(s, 0);
    return true;
}

struct ctx {
    struct src src;
    struct mat_render rc;
};

static void to_paige(void *p, const char *bytes, size_t len)
{
    paige_emit((paige_sink *)p, bytes, len);
}

static void noop_sink(void *p, const char *b, size_t l)
{
    (void)p;
    (void)b;
    (void)l;
}

static void term_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 &&
        ws.ws_col > 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

/* Render at most rows+1 visual lines to decide if the content fits one screen.
 * Lazy: stops as soon as it overflows, so a huge file peeks ~rows lines. */
static bool fits_one_screen(struct ctx *c, int rows, int cols)
{
    int visual = 0;
    for (size_t L = 0;; L++) {
        const unsigned char *d;
        size_t len;
        if (!src_line(&c->src, L, &d, &len))
            return true; /* reached EOF within the screen */
        visual += mat_render_line(&c->rc, (unsigned long)(L + 1), d, len, cols,
                                  noop_sink, NULL);
        if (visual > rows)
            return false;
    }
}

static int render_cb(void *vc, size_t L, int width, paige_sink *sink)
{
    struct ctx *c = vc;
    const unsigned char *d;
    size_t len;
    if (!src_line(&c->src, L, &d, &len))
        return 0;
    return mat_render_line(&c->rc, (unsigned long)(L + 1), d, len, width,
                           to_paige, sink);
}

int mat_page(const struct config *cfg, bool decorated)
{
    const char *name = cfg->nfiles ? cfg->files[0] : "-";
    bool is_stdin = false;
    int fd = mat_open_input(name, &is_stdin);
    if (fd < 0)
        return 0; /* error already reported; nothing to fall back to */

    struct ctx c;
    if (!src_open(&c.src, fd)) {
        mat_warn(is_stdin ? "stdin" : name);
        mat_close_input(fd, is_stdin, name);
        return 0;
    }

    unsigned style = 0;
    bool color = false;
    int tab_width = 0;
    if (decorated) {
        style = cfg->style & (MAT_S_NUMBERS | MAT_S_GRID);
        if (cfg->color == MAT_WHEN_ALWAYS)
            color = true;
        else if (cfg->color == MAT_WHEN_NEVER)
            color = false;
        else
            color = !mat_no_color();
        tab_width = cfg->tab_width < 0 ? 4 : cfg->tab_width;
    }
    mat_render_init(&c.rc, style, cfg->wrap, tab_width, color);

    /* When paging is auto, fall back to streaming the full frame for content
     * that fits on one screen (the peek is lazy, so a huge file stays instant).
     * --paging=always always enters the pager. */
    int ret = 0;
    if (cfg->paging != MAT_WHEN_ALWAYS) {
        int rows, cols;
        term_size(&rows, &cols);
        if (fits_one_screen(&c, rows - 1, cols))
            ret = 1; /* tell the caller to stream instead */
    }

    if (ret != 1) {
        char title[1040];
        snprintf(title, sizeof title, "File: %s", is_stdin ? "STDIN" : name);
        paige_doc doc = {&c, render_cb, title};
        paige_opts opts = {0}; /* we already handled the fits case */
        int r = paige_run(&doc, &opts);
        if (r < 0)
            ret = 1; /* no terminal after all: stream instead */
    }

    mat_render_free(&c.rc);
    src_free(&c.src);
    mat_close_input(fd, is_stdin, name);
    return ret;
}
