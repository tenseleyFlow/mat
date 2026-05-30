#include "interactive.h"
#include "err.h"
#include "frame.h"
#include "highlight.h"
#include "input.h"
#include "iobuf.h"
#include "render.h"
#include "scan.h"
#include "syntax.h"
#include "term.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define IP_BUFCAP ((size_t)(128 * 1024))

/* The streaming decorated printer. Content lines are rendered by render.c; this
 * module adds the frame chrome (header, grid rules) and buffers output. */
struct ip {
    bool header, header_size, rule;
    bool grid, color;
    int term_width;
    int panel_width;
    char *buf;
    size_t pos;
    char *pend; /* partial line carried across reads */
    size_t pend_cap, pend_len;
    bool failed;
    struct mat_render rc;
    const struct mat_rangeset *highlights; /* -H lines, or empty */
};

static void ip_flush(struct ip *p)
{
    if (p->pos && !p->failed) {
        if (mat_full_write(STDOUT_FILENO, p->buf, p->pos) < 0) {
            mat_warn("stdout");
            p->failed = true;
        }
    }
    p->pos = 0;
}

static void ip_write(struct ip *p, const char *d, size_t n)
{
    if (p->failed)
        return;
    if (n >= IP_BUFCAP) {
        ip_flush(p);
        if (!p->failed && mat_full_write(STDOUT_FILENO, d, n) < 0) {
            mat_warn("stdout");
            p->failed = true;
        }
        return;
    }
    if (p->pos + n > IP_BUFCAP)
        ip_flush(p);
    memcpy(p->buf + p->pos, d, n);
    p->pos += n;
}

static void ip_str(struct ip *p, const char *s)
{
    ip_write(p, s, strlen(s));
}

/* sink for render.c / frame.c: emit bytes into the output buffer. */
static void ip_sink(void *ctx, const char *bytes, size_t len)
{
    ip_write((struct ip *)ctx, bytes, len);
}

/* sink for render.c: emit one visual segment followed by a newline. */
static void stream_sink(void *ctx, const char *bytes, size_t len)
{
    struct ip *p = ctx;
    ip_write(p, bytes, len);
    ip_str(p, "\n");
}

static void hrule(struct ip *p, const char *junction)
{
    mat_frame_hrule(&p->rc, p->term_width, junction, ip_sink, p);
}

static void header_line(struct ip *p, const char *label, const char *value)
{
    mat_frame_header_line(&p->rc, label, value, ip_sink, p);
}

static void fmt_size(unsigned long long b, char *out, size_t n)
{
    if (b < 1024ULL)
        snprintf(out, n, "%llu B", b);
    else if (b < 1024ULL * 1024)
        snprintf(out, n, "%.1f KiB", (double)b / 1024.0);
    else if (b < 1024ULL * 1024 * 1024)
        snprintf(out, n, "%.1f MiB", (double)b / (1024.0 * 1024));
    else
        snprintf(out, n, "%.1f GiB", (double)b / (1024.0 * 1024 * 1024));
}

static void print_header(struct ip *p, const char *name, int fd, bool is_stdin)
{
    if (p->grid)
        hrule(p, BX_D);
    header_line(p, "File: ", is_stdin ? "STDIN" : name);
    if (p->header_size && !is_stdin) {
        struct stat st;
        if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
            char sz[48];
            fmt_size((unsigned long long)st.st_size, sz, sizeof sz);
            header_line(p, "Size: ", sz);
        }
    }
    if (p->grid)
        hrule(p, BX_X);
}

static void pend_append(struct ip *p, const unsigned char *d, size_t n)
{
    if (p->pend_len + n > p->pend_cap) {
        size_t cap = p->pend_cap ? p->pend_cap * 2 : 8192;
        while (cap < p->pend_len + n)
            cap *= 2;
        char *nb = realloc(p->pend, cap);
        if (nb == NULL) {
            mat_warnx("out of memory");
            p->failed = true;
            return;
        }
        p->pend = nb;
        p->pend_cap = cap;
    }
    memcpy(p->pend + p->pend_len, d, n);
    p->pend_len += n;
}

static void emit_line(struct ip *p, unsigned long n, const unsigned char *d,
                      size_t len)
{
    /* Streaming has no line total, so end-relative -H (-2:) can't resolve here;
     * absolute highlight ranges work. */
    const struct mat_rangeset *hl = p->highlights;
    p->rc.highlight = hl && hl->n > 0 && !hl->needs_total &&
                      mat_rangeset_contains(hl, (long)n, 0);
    mat_render_line(&p->rc, n, d, len, p->term_width, stream_sink, p);
}

static void print_body(struct ip *p, int fd, const char *name,
                       unsigned long *lineno)
{
    unsigned char rbuf[65536];
    p->pend_len = 0;
    for (;;) {
        ssize_t n = read(fd, rbuf, sizeof rbuf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            mat_warn(name);
            return;
        }
        if (n == 0)
            break;
        const unsigned char *s = rbuf;
        const unsigned char *end = rbuf + n;
        while (s < end) {
            const unsigned char *q = mat_scan_newline(s, end);
            if (q < end && p->pend_len == 0) {
                emit_line(p, (*lineno)++, s, (size_t)(q - s));
                s = q + 1;
            } else {
                pend_append(p, s, (size_t)(q - s));
                if (q < end) {
                    emit_line(p, (*lineno)++, (const unsigned char *)p->pend,
                              p->pend_len);
                    p->pend_len = 0;
                    s = q + 1;
                } else {
                    s = end;
                }
            }
            if (p->failed)
                return;
        }
    }
    if (p->pend_len > 0)
        emit_line(p, (*lineno)++, (const unsigned char *)p->pend, p->pend_len);
}

void mat_interactive_run(const struct config *cfg)
{
    struct ip p;
    memset(&p, 0, sizeof p);
    bool numbers = (cfg->style & MAT_S_NUMBERS) != 0;
    bool grid = (cfg->style & MAT_S_GRID) != 0;
    p.header = (cfg->style & MAT_S_HEADER) != 0;
    p.header_size = (cfg->style & MAT_S_HEADER_SIZE) != 0;
    p.rule = (cfg->style & MAT_S_RULE) != 0;
    p.term_width = mat_term_width(cfg->term_width);

    if (cfg->color == MAT_WHEN_ALWAYS)
        p.color = true;
    else if (cfg->color == MAT_WHEN_NEVER)
        p.color = false;
    else
        p.color = cfg->stdout_is_tty && !mat_no_color();

    int panel_width = numbers ? 5 : 0;
    if (panel_width > 0 && p.term_width < panel_width + 5) {
        numbers = false;
        grid = false;
        panel_width = 0;
    }
    p.grid = grid;
    p.panel_width = panel_width;

    unsigned eff_style =
        (numbers ? MAT_S_NUMBERS : 0u) | (grid ? MAT_S_GRID : 0u);
    int tab_width = cfg->tab_width < 0 ? 4 : cfg->tab_width;
    mat_render_init(&p.rc, eff_style, cfg->wrap, tab_width, p.color);
    p.highlights = &cfg->highlights;

    p.buf = malloc(IP_BUFCAP);
    if (p.buf == NULL) {
        mat_warnx("out of memory");
        mat_render_free(&p.rc);
        return;
    }

    static const char *const stdin_only[] = {"-"};
    const char *const *files = cfg->nfiles ? cfg->files : stdin_only;
    size_t nfiles = cfg->nfiles ? cfg->nfiles : 1;

    for (size_t i = 0; i < nfiles && !p.failed; i++) {
        bool is_stdin = false;
        int fd = mat_open_input(files[i], &is_stdin);
        if (fd < 0)
            continue;

        const char *label = is_stdin ? "stdin" : files[i];

        if (p.color) {
            const char *sname =
                is_stdin ? (cfg->file_name ? cfg->file_name : "") : files[i];
            p.rc.hl = mat_hl_open(mat_syntax_detect(cfg, sname, NULL, 0));
        }

        if (p.rule && i > 0)
            hrule(&p, BX_H);
        if (p.header)
            print_header(&p, label, fd, is_stdin);
        else if (p.grid)
            hrule(&p, BX_D);

        unsigned long lineno = 1;
        print_body(&p, fd, label, &lineno);

        if (p.grid)
            hrule(&p, BX_U);
        mat_hl_close(p.rc.hl);
        p.rc.hl = NULL;
        mat_close_input(fd, is_stdin, files[i]);
    }

    ip_flush(&p);
    free(p.buf);
    free(p.pend);
    mat_render_free(&p.rc);
}
