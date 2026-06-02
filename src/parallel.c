#include "parallel.h"
#include "ansi.h"
#include "err.h"
#include "frame.h"
#include "gitdiff.h"
#include "highlight.h"
#include "input.h"
#include "iobuf.h"
#include "linesrc.h"
#include "render.h"
#include "scan.h"
#include "syntax.h"
#include "term.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PAR_MIN_FILES 2
#define PAR_MAX_WORKERS 8

struct filebuf {
    char *data;
    size_t len, cap;
    bool failed;
};

static void fb_put(struct filebuf *fb, const char *d, size_t n)
{
    if (fb->failed)
        return;
    if (fb->len + n > fb->cap) {
        size_t nc = fb->cap ? fb->cap * 2 : 65536;
        while (nc < fb->len + n)
            nc *= 2;
        char *nb = realloc(fb->data, nc);
        if (nb == NULL) {
            fb->failed = true;
            return;
        }
        fb->data = nb;
        fb->cap = nc;
    }
    memcpy(fb->data + fb->len, d, n);
    fb->len += n;
}

static void fb_sink(void *ctx, const char *b, size_t n)
{
    fb_put((struct filebuf *)ctx, b, n);
}

static void fb_seg_sink(void *ctx, const char *b, size_t n)
{
    struct filebuf *fb = ctx;
    fb_put(fb, b, n);
    fb_put(fb, "\n", 1);
}

struct work {
    const struct config *cfg;
    const char *file;
    bool color;
    int term_width;
    unsigned rstyle;
    int tab_width;
    bool strip;
    struct filebuf out;
};

static void render_file(struct work *w)
{
    bool is_stdin = false;
    int fd = mat_open_input(w->file, &is_stdin);
    if (fd < 0)
        return;

    struct mat_linesrc src;
    if (!mat_linesrc_open(&src, fd)) {
        mat_warn(is_stdin ? "stdin" : w->file);
        mat_close_input(fd, is_stdin, w->file);
        return;
    }

    struct mat_render rc;
    mat_render_init(&rc, w->rstyle, w->cfg->wrap, w->tab_width, w->color);

    if (w->color) {
        const unsigned char *fl = (const unsigned char *)"";
        size_t fll = 0;
        mat_linesrc_line(&src, 0, &fl, &fll);
        rc.hl = mat_hl_open(mat_syntax_detect(w->cfg, w->file, fl, fll));
    }

    struct mat_changes chg;
    memset(&chg, 0, sizeof chg);
    if (w->cfg->diff && !is_stdin)
        mat_changes_load(&chg, w->file);
    rc.changes = chg.nlines > 0 ? &chg : NULL;

    /* Header. */
    bool header = (w->cfg->style & MAT_S_HEADER) != 0;
    bool header_size = (w->cfg->style & MAT_S_HEADER_SIZE) != 0;
    if (header) {
        if (rc.grid)
            mat_frame_hrule(&rc, w->term_width, BX_D, fb_sink, &w->out);
        mat_frame_header_line(&rc, "File: ", is_stdin ? "STDIN" : w->file,
                              fb_sink, &w->out);
        if (header_size && !is_stdin) {
            struct stat st;
            if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
                char sz[48];
                unsigned long long b = (unsigned long long)st.st_size;
                if (b < 1024ULL)
                    snprintf(sz, sizeof sz, "%llu B", b);
                else if (b < 1024ULL * 1024)
                    snprintf(sz, sizeof sz, "%.1f KiB", (double)b / 1024.0);
                else
                    snprintf(sz, sizeof sz, "%.1f MiB",
                             (double)b / (1024.0 * 1024));
                mat_frame_header_line(&rc, "Size: ", sz, fb_sink, &w->out);
            }
        }
        if (rc.grid)
            mat_frame_hrule(&rc, w->term_width, BX_X, fb_sink, &w->out);
    } else if (rc.grid) {
        mat_frame_hrule(&rc, w->term_width, BX_D, fb_sink, &w->out);
    }

    /* Content. */
    char *sbuf = NULL;
    size_t scap = 0;
    for (size_t L = 0;; L++) {
        const unsigned char *d;
        size_t len;
        if (!mat_linesrc_line(&src, L, &d, &len))
            break;
        if (w->strip && len > 0) {
            if (scap < len) {
                char *nb = realloc(sbuf, len);
                if (nb != NULL) {
                    sbuf = nb;
                    scap = len;
                }
            }
            if (scap >= len) {
                size_t sn = mat_strip_ansi(d, len, sbuf);
                d = (const unsigned char *)sbuf;
                len = sn;
            }
        }
        const struct mat_rangeset *hl = &w->cfg->highlights;
        rc.highlight = hl->n > 0 && mat_rangeset_contains(hl, (long)(L + 1), 0);
        mat_render_line(&rc, (unsigned long)(L + 1), d, len, w->term_width,
                        fb_seg_sink, &w->out);
    }
    free(sbuf);

    /* Footer. */
    if (rc.grid)
        mat_frame_hrule(&rc, w->term_width, BX_U, fb_sink, &w->out);

    rc.changes = NULL;
    mat_changes_free(&chg);
    mat_hl_close(rc.hl);
    mat_render_free(&rc);
    mat_linesrc_free(&src);
    mat_close_input(fd, is_stdin, w->file);
}

static void *worker(void *arg)
{
    render_file((struct work *)arg);
    return NULL;
}

bool mat_parallel_run(const struct config *cfg)
{
    if (cfg->nfiles < PAR_MIN_FILES)
        return false;

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
    if (panel > 0 && tw < panel + 5) {
        numbers = false;
        grid = false;
    }
    unsigned rstyle = (numbers ? MAT_S_NUMBERS : 0u) | (grid ? MAT_S_GRID : 0u);
    int tab_width = cfg->tab_width < 0 ? 4 : cfg->tab_width;

    size_t n = cfg->nfiles;
    struct work *jobs = calloc(n, sizeof *jobs);
    if (jobs == NULL)
        return false;

    for (size_t i = 0; i < n; i++) {
        jobs[i].cfg = cfg;
        jobs[i].file = cfg->files[i];
        jobs[i].color = color;
        jobs[i].term_width = tw;
        jobs[i].rstyle = rstyle;
        jobs[i].tab_width = tab_width;
        jobs[i].strip = cfg->strip_ansi == MAT_WHEN_ALWAYS ||
                        (cfg->strip_ansi == MAT_WHEN_AUTO && color);
    }

    /* Launch workers in batches of PAR_MAX_WORKERS. */
    for (size_t base = 0; base < n; base += PAR_MAX_WORKERS) {
        size_t batch = n - base;
        if (batch > PAR_MAX_WORKERS)
            batch = PAR_MAX_WORKERS;

        pthread_t thr[PAR_MAX_WORKERS];
        bool launched[PAR_MAX_WORKERS];
        for (size_t i = 0; i < batch; i++) {
            if (pthread_create(&thr[i], NULL, worker, &jobs[base + i]) == 0) {
                launched[i] = true;
            } else {
                launched[i] = false;
                render_file(&jobs[base + i]);
            }
        }
        for (size_t i = 0; i < batch; i++)
            if (launched[i])
                pthread_join(thr[i], NULL);
    }

    /* Write in input order, inserting a rule separator between files when the
     * style includes 'rule'. */
    bool rule = (cfg->style & MAT_S_RULE) != 0;
    struct filebuf sep = {NULL, 0, 0, false};
    if (rule) {
        struct mat_render tmp;
        mat_render_init(&tmp, rstyle, cfg->wrap, tab_width, color);
        mat_frame_hrule(&tmp, tw, BX_H, fb_sink, &sep);
        mat_render_free(&tmp);
    }
    for (size_t i = 0; i < n; i++) {
        if (rule && i > 0 && sep.len > 0)
            mat_pipe_write(STDOUT_FILENO, sep.data, sep.len);
        if (jobs[i].out.failed) {
            mat_warn(jobs[i].file);
        } else if (jobs[i].out.len > 0) {
            mat_pipe_write(STDOUT_FILENO, jobs[i].out.data, jobs[i].out.len);
        }
        free(jobs[i].out.data);
    }
    free(sep.data);

    free(jobs);
    return true;
}
