#include "rangeprint.h"
#include "ansi.h"
#include "err.h"
#include "frame.h"
#include "highlight.h"
#include "input.h"
#include "iobuf.h"
#include "linesrc.h"
#include "range.h"
#include "render.h"
#include "scan.h"
#include "syntax.h"
#include "term.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define RP_BUFCAP ((size_t)(128 * 1024))

/* ---- buffered stdout ---- */

struct out {
    char *buf;
    size_t pos;
    bool failed;
};

static void out_flush(struct out *o)
{
    if (o->pos && !o->failed) {
        if (mat_full_write(STDOUT_FILENO, o->buf, o->pos) < 0) {
            mat_warn("stdout");
            o->failed = true;
        }
    }
    o->pos = 0;
}

static void out_put(struct out *o, const char *d, size_t n)
{
    if (o->failed)
        return;
    if (n >= RP_BUFCAP) {
        out_flush(o);
        if (!o->failed && mat_full_write(STDOUT_FILENO, d, n) < 0) {
            mat_warn("stdout");
            o->failed = true;
        }
        return;
    }
    if (o->pos + n > RP_BUFCAP)
        out_flush(o);
    memcpy(o->buf + o->pos, d, n);
    o->pos += n;
}

static void out_sink(void *ctx, const char *b, size_t n) /* frame chrome */
{
    out_put((struct out *)ctx, b, n);
}

static void seg_sink(void *ctx, const char *b,
                     size_t n) /* one render segment */
{
    struct out *o = ctx;
    out_put(o, b, n);
    out_put(o, "\n", 1);
}

/* ---- the line emitter (shared by the seekable and streaming paths) ---- */

struct emit {
    struct out *o;
    bool decorated;
    struct mat_render rc; /* set up when decorated */
    int term_width;
    const struct mat_rangeset *highlights;
    long prev;   /* last emitted line number, for snip detection */
    bool strip;  /* strip input ANSI escapes before emitting */
    char *sbuf;  /* reused strip buffer */
    size_t scap; /* its capacity */
};

static void emit_setup(struct emit *e, const struct config *cfg, bool decorated,
                       struct out *o)
{
    memset(e, 0, sizeof *e);
    e->o = o;
    e->decorated = decorated;
    e->highlights = &cfg->highlights;
    /* auto strips only under decorations; always/never force it either way. */
    e->strip = cfg->strip_ansi == MAT_WHEN_ALWAYS ||
               (cfg->strip_ansi == MAT_WHEN_AUTO && decorated);
    if (!decorated)
        return;

    bool color;
    if (cfg->color == MAT_WHEN_ALWAYS)
        color = true;
    else if (cfg->color == MAT_WHEN_NEVER)
        color = false;
    else
        color = cfg->stdout_is_tty && !mat_no_color();

    int tw = mat_term_width(cfg->term_width);
    bool numbers = (cfg->style & MAT_S_NUMBERS) != 0;
    bool grid = (cfg->style & MAT_S_GRID) != 0;
    int panel = numbers ? 5 : 0;
    if (panel > 0 && tw < panel + 5) { /* too narrow for a gutter */
        numbers = false;
        grid = false;
    }
    unsigned rstyle = (numbers ? MAT_S_NUMBERS : 0u) | (grid ? MAT_S_GRID : 0u);
    int tab_width = cfg->tab_width < 0 ? 4 : cfg->tab_width;
    mat_render_init(&e->rc, rstyle, cfg->wrap, tab_width, color);
    e->term_width = tw;
}

static void emit_free(struct emit *e)
{
    if (e->decorated)
        mat_render_free(&e->rc);
    free(e->sbuf);
}

static void emit_header(struct emit *e, const struct config *cfg,
                        const char *name, bool is_stdin)
{
    if (!e->decorated)
        return;
    bool header = (cfg->style & MAT_S_HEADER) != 0;
    if (header) {
        if (e->rc.grid)
            mat_frame_hrule(&e->rc, e->term_width, BX_D, out_sink, e->o);
        mat_frame_header_line(&e->rc, "File: ", is_stdin ? "STDIN" : name,
                              out_sink, e->o);
        if (e->rc.grid)
            mat_frame_hrule(&e->rc, e->term_width, BX_X, out_sink, e->o);
    } else if (e->rc.grid) {
        mat_frame_hrule(&e->rc, e->term_width, BX_D, out_sink, e->o);
    }
}

static void emit_footer(struct emit *e)
{
    if (e->decorated && e->rc.grid)
        mat_frame_hrule(&e->rc, e->term_width, BX_U, out_sink, e->o);
}

/* Emit one selected line, inserting a snip marker where the printed ranges are
 * disjoint (decorated output only). */
static void emit_line(struct emit *e, long L, const unsigned char *d,
                      size_t len, long total)
{
    if (e->strip && len > 0) {
        if (e->scap < len) {
            char *nb = realloc(e->sbuf, len);
            if (nb != NULL) {
                e->sbuf = nb;
                e->scap = len;
            }
        }
        if (e->scap >= len) {
            size_t sn = mat_strip_ansi(d, len, e->sbuf);
            d = (const unsigned char *)e->sbuf;
            len = sn;
        }
    }
    if (e->decorated) {
        if (e->prev != 0 && L > e->prev + 1)
            mat_frame_header_line(&e->rc, "...", "", out_sink, e->o);
        e->rc.highlight = e->highlights->n > 0 &&
                          mat_rangeset_contains(e->highlights, L, total);
        mat_render_line(&e->rc, (unsigned long)L, d, len, e->term_width,
                        seg_sink, e->o);
    } else {
        out_put(e->o, (const char *)d, len);
        out_put(e->o, "\n", 1);
    }
    e->prev = L;
}

/* ---- seekable path: random access over the mmap'd line index ---- */

static void print_seekable(const struct config *cfg, struct mat_linesrc *src,
                           struct emit *e)
{
    long total = 0;
    if (cfg->ranges.needs_total || cfg->highlights.needs_total)
        total = (long)mat_linesrc_total(src);
    long maxl = mat_rangeset_max_line(&cfg->ranges);
    for (long L = 1; L <= maxl && !e->o->failed; L++) {
        const unsigned char *d;
        size_t len;
        if (!mat_linesrc_line(src, (size_t)(L - 1), &d, &len))
            break;
        if (!mat_rangeset_contains(&cfg->ranges, L, total))
            continue;
        emit_line(e, L, d, len, total);
    }
}

/* ---- streaming path: a ring of the last N lines, no whole-file buffering ----
 *
 * For a non-seekable input we can't seek, so a last-N range can't be resolved
 * until EOF. Absolute/open-ended ranges are emitted as their lines stream past;
 * a ring of the largest "last N" holds just the tail, which is flushed
 * (skipping anything already emitted) once the total line count is known. */

struct rslot {
    char *buf;
    size_t cap, len;
    long lineno;
};

struct ring {
    struct rslot *slot;
    int cap, count, head;
};

static void ring_init(struct ring *r, int cap)
{
    r->cap = cap;
    r->count = 0;
    r->head = 0;
    r->slot = cap > 0 ? calloc((size_t)cap, sizeof *r->slot) : NULL;
}

static void ring_push(struct ring *r, long lineno, const unsigned char *d,
                      size_t len)
{
    if (r->cap == 0 || r->slot == NULL)
        return;
    int idx;
    if (r->count == r->cap) {
        idx = r->head; /* overwrite the oldest */
        r->head = (r->head + 1) % r->cap;
    } else {
        idx = (r->head + r->count) % r->cap;
        r->count++;
    }
    struct rslot *s = &r->slot[idx];
    if (s->cap < len) {
        char *nb = realloc(s->buf, len ? len : 1);
        if (nb == NULL)
            return;
        s->buf = nb;
        s->cap = len;
    }
    memcpy(s->buf, d, len);
    s->len = len;
    s->lineno = lineno;
}

static void ring_free(struct ring *r)
{
    for (int i = 0; i < r->cap; i++)
        free(r->slot[i].buf);
    free(r->slot);
}

/* Grow-and-append a partial line carried across read boundaries. */
static char *pend_append(char *pend, size_t *cap, size_t *plen,
                         const unsigned char *d, size_t n)
{
    if (*plen + n > *cap) {
        size_t nc = *cap ? *cap * 2 : 8192;
        while (nc < *plen + n)
            nc *= 2;
        char *nb = realloc(pend, nc);
        if (nb == NULL)
            return pend; /* drop on OOM; len unchanged */
        pend = nb;
        *cap = nc;
    }
    memcpy(pend + *plen, d, n);
    *plen += n;
    return pend;
}

static void print_stream(const struct config *cfg, int fd, const char *name,
                         struct emit *e)
{
    long max_line = mat_rangeset_max_line(&cfg->ranges);
    bool bounded = max_line != LONG_MAX; /* no open-ended or last-N range */
    struct ring ring;
    ring_init(&ring, cfg->ranges.max_tail > 0 ? (int)cfg->ranges.max_tail : 0);

    unsigned char rbuf[65536];
    char *pend = NULL;
    size_t pend_cap = 0, pend_len = 0;
    long L = 0;
    bool done = false;

    while (!done && !e->o->failed) {
        ssize_t n = read(fd, rbuf, sizeof rbuf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            mat_warn(name);
            break;
        }
        if (n == 0)
            break;
        const unsigned char *s = rbuf, *end = rbuf + n;
        while (s < end) {
            const unsigned char *q = mat_scan_newline(s, end);
            if (q == end) { /* no newline yet: stash the remainder */
                pend =
                    pend_append(pend, &pend_cap, &pend_len, s, (size_t)(q - s));
                break;
            }
            const unsigned char *line;
            size_t len;
            if (pend_len > 0) {
                pend =
                    pend_append(pend, &pend_cap, &pend_len, s, (size_t)(q - s));
                line = (const unsigned char *)pend;
                len = pend_len;
                pend_len = 0;
            } else {
                line = s;
                len = (size_t)(q - s);
            }
            s = q + 1;
            L++;
            if (bounded && L > max_line) {
                done = true;
                break;
            }
            if (mat_rangeset_abs_contains(&cfg->ranges, L))
                emit_line(e, L, line, len, 0);
            ring_push(&ring, L, line, len);
        }
    }
    /* A final line with no trailing newline. */
    if (!done && pend_len > 0 && !e->o->failed) {
        L++;
        if (!bounded || L <= max_line) {
            if (mat_rangeset_abs_contains(&cfg->ranges, L))
                emit_line(e, L, (const unsigned char *)pend, pend_len, 0);
            ring_push(&ring, L, (const unsigned char *)pend, pend_len);
        }
    }

    /* Flush the tail: last-N lines not already emitted by an absolute range. */
    if (ring.count > 0 && !e->o->failed) {
        long total = L;
        for (int i = 0; i < ring.count && !e->o->failed; i++) {
            struct rslot *sl = &ring.slot[(ring.head + i) % ring.cap];
            if (mat_rangeset_rel_contains(&cfg->ranges, sl->lineno, total) &&
                !mat_rangeset_abs_contains(&cfg->ranges, sl->lineno))
                emit_line(e, sl->lineno, (const unsigned char *)sl->buf,
                          sl->len, total);
        }
    }

    ring_free(&ring);
    free(pend);
}

/* ---- per-file dispatch ---- */

static void print_file(const struct config *cfg, const char *file,
                       bool decorated, struct out *o)
{
    bool is_stdin = false;
    int fd = mat_open_input(file, &is_stdin);
    if (fd < 0)
        return;

    struct stat st;
    bool seekable = fstat(fd, &st) == 0 && S_ISREG(st.st_mode);

    struct emit e;
    emit_setup(&e, cfg, decorated, o);
    emit_header(&e, cfg, file, is_stdin);

    if (seekable) {
        struct mat_linesrc src;
        if (mat_linesrc_open(&src, fd)) {
            /* A decorated binary file is skipped with a notice unless the user
             * asked for as-text. linesrc has already decoded any UTF-16. */
            if (decorated && src.encoding == MAT_ENC_BINARY &&
                cfg->binary == MAT_BINARY_NO_PRINTING)
                mat_frame_header_line(&e.rc, "<BINARY> ",
                                      "(--binary=as-text to show)", out_sink,
                                      o);
            else {
                /* Resolve a syntax and open a highlighter when coloring. */
                if (e.rc.color) {
                    const unsigned char *fl = (const unsigned char *)"";
                    size_t fll = 0;
                    mat_linesrc_line(&src, 0, &fl, &fll);
                    const char *sname =
                        is_stdin ? (cfg->file_name ? cfg->file_name : "")
                                 : file;
                    e.rc.hl =
                        mat_hl_open(mat_syntax_detect(cfg, sname, fl, fll));
                }
                print_seekable(cfg, &src, &e);
                mat_hl_close(e.rc.hl);
                e.rc.hl = NULL;
            }
            mat_linesrc_free(&src);
        } else {
            mat_warn(is_stdin ? "stdin" : file);
        }
    } else {
        print_stream(cfg, fd, is_stdin ? "stdin" : file, &e);
    }

    emit_footer(&e);
    emit_free(&e);
    mat_close_input(fd, is_stdin, file);
}

void mat_rangeprint_run(const struct config *cfg, bool decorated)
{
    struct out o = {malloc(RP_BUFCAP), 0, false};
    if (o.buf == NULL) {
        mat_warnx("out of memory");
        return;
    }
    static const char *const stdin_only[] = {"-"};
    const char *const *files = cfg->nfiles ? cfg->files : stdin_only;
    size_t nfiles = cfg->nfiles ? cfg->nfiles : 1;
    for (size_t i = 0; i < nfiles && !o.failed; i++)
        print_file(cfg, files[i], decorated, &o);
    out_flush(&o);
    free(o.buf);
}
