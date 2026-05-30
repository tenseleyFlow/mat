#include "rangeprint.h"
#include "err.h"
#include "frame.h"
#include "input.h"
#include "iobuf.h"
#include "linesrc.h"
#include "range.h"
#include "render.h"
#include "term.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RP_BUFCAP ((size_t)(128 * 1024))

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

/* frame.c / chrome sink: raw bytes. */
static void out_sink(void *ctx, const char *b, size_t n)
{
    out_put((struct out *)ctx, b, n);
}

/* render.c sink: one visual segment then a newline. */
static void seg_sink(void *ctx, const char *b, size_t n)
{
    struct out *o = ctx;
    out_put(o, b, n);
    out_put(o, "\n", 1);
}

/* Resolve the iteration bound and total once per file. A bounded selection
 * stops at its highest line; open-ended or last-N must read to EOF. */
static long bound_and_total(const struct config *cfg, struct mat_linesrc *src,
                            long *total)
{
    *total = 0;
    if (cfg->ranges.needs_total)
        *total = (long)mat_linesrc_total(src);
    return mat_rangeset_max_line(&cfg->ranges);
}

static void print_plain(const struct config *cfg, struct mat_linesrc *src,
                        struct out *o)
{
    long total;
    long maxl = bound_and_total(cfg, src, &total);
    for (long L = 1; L <= maxl && !o->failed; L++) {
        const unsigned char *d;
        size_t len;
        if (!mat_linesrc_line(src, (size_t)(L - 1), &d, &len))
            break;
        if (!mat_rangeset_contains(&cfg->ranges, L, total))
            continue;
        out_put(o, (const char *)d, len);
        out_put(o, "\n", 1);
    }
}

/* A snip marker shown where the printed ranges are disjoint. */
static void snip(struct mat_render *rc, int term_width, struct out *o)
{
    (void)term_width;
    mat_frame_header_line(rc, "...", "", out_sink, o);
}

static void print_decorated(const struct config *cfg, struct mat_linesrc *src,
                            const char *name, bool is_stdin, struct out *o)
{
    bool color;
    if (cfg->color == MAT_WHEN_ALWAYS)
        color = true;
    else if (cfg->color == MAT_WHEN_NEVER)
        color = false;
    else
        color = cfg->stdout_is_tty && !mat_no_color();

    int term_width = mat_term_width(cfg->term_width);
    bool numbers = (cfg->style & MAT_S_NUMBERS) != 0;
    bool grid = (cfg->style & MAT_S_GRID) != 0;
    bool header = (cfg->style & MAT_S_HEADER) != 0;
    int panel = numbers ? 5 : 0;
    if (panel > 0 && term_width < panel + 5) { /* too narrow for a gutter */
        numbers = false;
        grid = false;
    }

    struct mat_render rc;
    unsigned rstyle = (numbers ? MAT_S_NUMBERS : 0u) | (grid ? MAT_S_GRID : 0u);
    int tab_width = cfg->tab_width < 0 ? 4 : cfg->tab_width;
    mat_render_init(&rc, rstyle, cfg->wrap, tab_width, color);

    if (header) {
        if (grid)
            mat_frame_hrule(&rc, term_width, BX_D, out_sink, o);
        mat_frame_header_line(&rc, "File: ", is_stdin ? "STDIN" : name,
                              out_sink, o);
        if (grid)
            mat_frame_hrule(&rc, term_width, BX_X, out_sink, o);
    } else if (grid) {
        mat_frame_hrule(&rc, term_width, BX_D, out_sink, o);
    }

    long total;
    long maxl = bound_and_total(cfg, src, &total);
    long prev = 0;
    for (long L = 1; L <= maxl && !o->failed; L++) {
        const unsigned char *d;
        size_t len;
        if (!mat_linesrc_line(src, (size_t)(L - 1), &d, &len))
            break;
        if (!mat_rangeset_contains(&cfg->ranges, L, total))
            continue;
        if (prev != 0 && L > prev + 1)
            snip(&rc, term_width, o);
        mat_render_line(&rc, (unsigned long)L, d, len, term_width, seg_sink, o);
        prev = L;
    }

    if (grid)
        mat_frame_hrule(&rc, term_width, BX_U, out_sink, o);
    mat_render_free(&rc);
}

static void print_file(const struct config *cfg, const char *file,
                       bool decorated, struct out *o)
{
    bool is_stdin = false;
    int fd = mat_open_input(file, &is_stdin);
    if (fd < 0)
        return;
    struct mat_linesrc src;
    if (!mat_linesrc_open(&src, fd)) {
        mat_warn(is_stdin ? "stdin" : file);
        mat_close_input(fd, is_stdin, file);
        return;
    }
    if (decorated)
        print_decorated(cfg, &src, file, is_stdin, o);
    else
        print_plain(cfg, &src, o);
    mat_linesrc_free(&src);
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
