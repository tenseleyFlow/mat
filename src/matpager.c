#include "matpager.h"
#include "err.h"
#include "highlight.h"
#include "input.h"
#include "linesrc.h"
#include "render.h"
#include "syntax.h"
#include "term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "paige.h"

struct ctx {
    struct mat_linesrc src;
    struct mat_render rc;
    const struct config *cfg;
    struct mat_changes chg;
    /* Cached lexer entry-state per line, so syntax highlighting is correct
     * regardless of the pager's render order. hl_entry[i] is the multi-line
     * lexer state entering line i; hl_entry[0] is 0 (document start). */
    int *hl_entry;
    size_t hl_entry_n;
    size_t hl_entry_cap;
};

/* Make hl_entry[L] valid by lexing forward from the furthest line already
 * cached, threading state. Multi-line constructs (``` fences, block comments)
 * mean a line's coloring depends on every line above it; the pager renders
 * lines out of order and repeatedly, so we restore each line's true entry state
 * here instead of trusting whatever the previous render call left behind. */
static void ensure_entry(struct ctx *c, size_t L)
{
    if (c->rc.hl == NULL)
        return;
    if (c->hl_entry_n == 0) {
        if (c->hl_entry_cap == 0) {
            c->hl_entry_cap = 1024;
            c->hl_entry = malloc(c->hl_entry_cap * sizeof *c->hl_entry);
            if (c->hl_entry == NULL) {
                c->hl_entry_cap = 0;
                return;
            }
        }
        c->hl_entry[0] = 0; /* HL_NORMAL at the document start */
        c->hl_entry_n = 1;
    }
    while (c->hl_entry_n <= L) {
        size_t i = c->hl_entry_n - 1; /* lex line i to learn line i+1's entry */
        const unsigned char *d;
        size_t len;
        if (!mat_linesrc_line(&c->src, i, &d, &len))
            break;                   /* EOF: nothing more to cache */
        struct mat_span scratch[64]; /* span output discarded; we want state */
        mat_hl_set_state(c->rc.hl, c->hl_entry[i]);
        mat_hl_line(c->rc.hl, d, len, scratch, 64);
        if (c->hl_entry_n == c->hl_entry_cap) {
            size_t nc = c->hl_entry_cap * 2;
            int *nb = realloc(c->hl_entry, nc * sizeof *nb);
            if (nb == NULL)
                break;
            c->hl_entry = nb;
            c->hl_entry_cap = nc;
        }
        c->hl_entry[c->hl_entry_n++] = mat_hl_get_state(c->rc.hl);
    }
}

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
        if (!mat_linesrc_line(&c->src, L, &d, &len))
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
    if (!mat_linesrc_line(&c->src, L, &d, &len))
        return 0;
    /* Restore the correct multi-line lexer state for THIS line before rendering
     * (the render path lexes the line internally), so highlighting is identical
     * no matter what the pager rendered before this call. */
    if (c->rc.hl != NULL) {
        ensure_entry(c, L);
        if (L < c->hl_entry_n)
            mat_hl_set_state(c->rc.hl, c->hl_entry[L]);
    }
    long line1 = (long)(L + 1);
    const struct mat_rangeset *hl = c->cfg ? &c->cfg->highlights : NULL;
    c->rc.highlight = hl && hl->n > 0 && mat_rangeset_contains(hl, line1, 0);
    return mat_render_line(&c->rc, (unsigned long)(L + 1), d, len, width,
                           to_paige, sink);
}

/* Name the last line so jump-to-bottom is O(screen). We deliberately do not
 * provide line_count (it would force a full scan to answer %, paid up front);
 * seek_end lets paige reach EOF lazily — it indexes to the end only when the
 * reader actually presses G, without rendering every line on the way. */
static int seek_end_cb(void *vc, size_t *out)
{
    struct ctx *c = vc;
    size_t total = mat_linesrc_total(&c->src);
    if (total == 0)
        return 0;
    *out = total - 1;
    return 1;
}

int mat_page(const struct config *cfg, bool decorated)
{
    const char *name = cfg->nfiles ? cfg->files[0] : "-";
    bool is_stdin = false;
    int fd = mat_open_input(name, &is_stdin);
    if (fd < 0)
        return 0; /* error already reported; nothing to fall back to */

    struct ctx c;
    memset(&c, 0, sizeof c); /* zero hl_entry cache + the rest before init */
    if (!mat_linesrc_open(&c.src, fd)) {
        mat_warn(is_stdin ? "stdin" : name);
        mat_close_input(fd, is_stdin, name);
        return 0;
    }

    /* A binary file is skipped with a notice unless the user asked for as-text;
     * linesrc has already decoded any UTF-16. */
    if (decorated && c.src.encoding == MAT_ENC_BINARY &&
        cfg->binary == MAT_BINARY_NO_PRINTING) {
        printf("%s: binary file (--binary=as-text to show)\n",
               is_stdin ? "STDIN" : name);
        mat_linesrc_free(&c.src);
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
    c.cfg = cfg;

    memset(&c.chg, 0, sizeof c.chg);
    if (cfg->diff && !is_stdin)
        mat_changes_load(&c.chg, name);
    c.rc.changes = c.chg.nlines > 0 ? &c.chg : NULL;

    if (color) {
        const unsigned char *fl = (const unsigned char *)"";
        size_t fll = 0;
        mat_linesrc_line(&c.src, 0, &fl, &fll);
        const char *sname =
            is_stdin ? (cfg->file_name ? cfg->file_name : "") : name;
        c.rc.hl = mat_hl_open(mat_syntax_detect(cfg, sname, fl, fll));
    }

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
        /* Designated init so added paige_doc fields stay zeroed (and quiet
         * under -Wmissing-field-initializers as the paige API grows). */
        paige_doc doc = {.ctx = &c,
                         .render_line = render_cb,
                         .seek_end = seek_end_cb,
                         .title = title};
        paige_opts opts = {0}; /* we already handled the fits case */
        int r = paige_run(&doc, &opts);
        if (r < 0)
            ret = 1; /* no terminal after all: stream instead */
    }

    c.rc.changes = NULL;
    mat_changes_free(&c.chg);
    mat_hl_close(c.rc.hl);
    mat_render_free(&c.rc);
    mat_linesrc_free(&c.src);
    free(c.hl_entry);
    mat_close_input(fd, is_stdin, name);
    return ret;
}
