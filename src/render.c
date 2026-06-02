#include "render.h"
#include "width.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mat_render_init(struct mat_render *r, unsigned style, enum mat_wrap wrap,
                     int tab_width, bool color)
{
    memset(r, 0, sizeof *r);
    r->numbers = (style & MAT_S_NUMBERS) != 0;
    r->grid = (style & MAT_S_GRID) != 0;
    r->color = color;
    r->panel_width = r->numbers ? 5 : 0;
    r->wrap = wrap;
    r->tab_width = tab_width;
    r->wbuf = malloc(4096);
    r->wbuf_cap = r->wbuf ? 4096 : 0;
    r->seg = malloc(1024);
    r->seg_cap = r->seg ? 1024 : 0;
}

void mat_render_free(struct mat_render *r)
{
    free(r->wbuf);
    free(r->seg);
    free(r->spans);
}

int mat_render_content_width(const struct mat_render *r, int width)
{
    int gutter_vis = r->numbers ? (r->panel_width + (r->grid ? 2 : 0)) : 0;
    int cw = width - gutter_vis;
    return cw < 1 ? 1 : cw;
}

/* ---- assembly buffers ---- */

#define MAT_BUF_LIMIT ((size_t)(16 * 1024 * 1024))

static void wbuf_append(struct mat_render *r, const char *d, size_t n)
{
    if (r->failed)
        return;
    if (r->wbuf_len + n > r->wbuf_cap) {
        size_t cap = r->wbuf_cap ? r->wbuf_cap * 2 : 8192;
        while (cap < r->wbuf_len + n)
            cap *= 2;
        if (cap > MAT_BUF_LIMIT) {
            r->failed = true;
            return;
        }
        char *nb = realloc(r->wbuf, cap);
        if (!nb) {
            r->failed = true;
            return;
        }
        r->wbuf = nb;
        r->wbuf_cap = cap;
    }
    memcpy(r->wbuf + r->wbuf_len, d, n);
    r->wbuf_len += n;
}

static void seg_append(struct mat_render *r, const char *d, size_t n)
{
    if (r->failed)
        return;
    if (r->seg_len + n > r->seg_cap) {
        size_t cap = r->seg_cap ? r->seg_cap * 2 : 256;
        while (cap < r->seg_len + n)
            cap *= 2;
        if (cap > MAT_BUF_LIMIT) {
            r->failed = true;
            return;
        }
        char *nb = realloc(r->seg, cap);
        if (!nb) {
            r->failed = true;
            return;
        }
        r->seg = nb;
        r->seg_cap = cap;
    }
    memcpy(r->seg + r->seg_len, d, n);
    r->seg_len += n;
}

static void seg_str(struct mat_render *r, const char *s)
{
    seg_append(r, s, strlen(s));
}

/* Expand tabs in [d,len) into r->wbuf, advancing through display columns. */
static void expand_tabs(struct mat_render *r, const unsigned char *d,
                        size_t len)
{
    r->wbuf_len = 0;
    if (r->tab_width <= 0 || memchr(d, '\t', len) == NULL) {
        wbuf_append(r, (const char *)d, len);
        return;
    }
    int col = 0;
    size_t i = 0;
    while (i < len) {
        if (d[i] == '\t') {
            static const char spaces[] = "                "
                                         "                "
                                         "                "
                                         "                ";
            int sp = r->tab_width - (col % r->tab_width);
            wbuf_append(r, spaces, (size_t)sp);
            col += sp;
            i++;
        } else if (d[i] < 0x80) {
            wbuf_append(r, (const char *)d + i, 1);
            col += 1;
            i++;
        } else {
            uint32_t cp;
            size_t cl = mat_utf8_decode(d + i, d + len, &cp);
            wbuf_append(r, (const char *)d + i, cl);
            col += mat_wcwidth(cp);
            i += cl;
        }
    }
}

static int run_width(const unsigned char *a, const unsigned char *b)
{
    int w = 0;
    while (a < b) {
        uint32_t cp;
        size_t cl = mat_utf8_decode(a, b, &cp);
        w += mat_wcwidth(cp);
        a += cl;
    }
    return w;
}

/* Build the gutter prefix (number/spaces + grid separator) into r->seg. */
static void put_gutter(struct mat_render *r, unsigned long n, bool continuation)
{
    if (r->color)
        seg_str(r, COL_GUTTER);
    if (r->numbers) {
        if (continuation) {
            for (int i = 0; i < r->panel_width; i++)
                seg_append(r, " ", 1);
        } else {
            char num[] = "       ";
            int w = r->panel_width - 1;
            if (w > 7)
                w = 7;
            int i = w - 1;
            unsigned long v = n;
            do {
                num[i--] = '0' + (char)(v % 10);
                v /= 10;
            } while (v && i >= 0);
            seg_append(r, num, (size_t)w);
            seg_append(r, " ", 1);
        }
    }
    if (r->changes && !continuation) {
        enum mat_change chg = MAT_CHG_NONE;
        if (n > 0 && n <= r->changes->nlines)
            chg = r->changes->line[n];
        if (r->color) {
            const char *cc = mat_change_color(chg);
            if (cc[0])
                seg_str(r, cc);
            seg_str(r, mat_change_marker(chg));
            if (cc[0])
                seg_str(r, COL_GUTTER);
        } else {
            seg_str(r, mat_change_marker(chg));
        }
    }
    if (r->grid && r->panel_width > 0)
        seg_str(r, BX_V " ");
    if (r->color)
        seg_str(r, COL_RESET);
}

#define SGR_FG_DEFAULT "\x1b[39m" /* reset foreground without touching bg */

/* Emit wbuf[woff, woff+clen) with per-span syntax colors. Foreground SGR only,
 * so a -H background (if any) survives; the caller resets at the segment end.
 */
static void emit_colored(struct mat_render *r, size_t woff, size_t clen)
{
    size_t end = woff + clen, pos = woff;
    int si = 0;
    while (si < r->nspans &&
           (size_t)r->spans[si].start + r->spans[si].len <= pos)
        si++;
    const char *last = SGR_FG_DEFAULT; /* fg is default at segment start */
    while (pos < end) {
        const char *sgr;
        size_t run_end;
        if (si >= r->nspans || pos < r->spans[si].start) {
            run_end = (si < r->nspans && (size_t)r->spans[si].start < end)
                          ? r->spans[si].start
                          : end;
            sgr = SGR_FG_DEFAULT;
        } else {
            struct mat_span *sp = &r->spans[si];
            size_t sp_end = (size_t)sp->start + sp->len;
            run_end = sp_end < end ? sp_end : end;
            sgr = mat_theme_sgr(sp->tok);
            if (sgr[0] == '\0')
                sgr = SGR_FG_DEFAULT; /* uncolored token: reset fg */
            if (run_end >= sp_end)
                si++;
        }
        if (sgr != last) {
            seg_str(r, sgr);
            last = sgr;
        }
        seg_append(r, r->wbuf + pos, run_end - pos);
        pos = run_end;
    }
}

static void emit_seg(struct mat_render *r, unsigned long n, bool continuation,
                     const char *content, size_t clen, size_t woff,
                     mat_sink_fn sink, void *ctx)
{
    r->seg_len = 0;
    put_gutter(r, n, continuation);
    bool bg = r->highlight && r->color;
    if (bg)
        seg_str(r, COL_HL);
    if (r->hl_on) {
        emit_colored(r, woff, clen);
        seg_str(r, COL_RESET);
    } else {
        seg_append(r, content, clen);
        if (bg)
            seg_str(r, COL_RESET);
    }
    sink(ctx, r->seg, r->seg_len);
}

int mat_render_line(struct mat_render *r, unsigned long lineno,
                    const unsigned char *d, size_t len, int width,
                    mat_sink_fn sink, void *ctx)
{
    /* Tokenize the *original* line so lexers see literal tabs. */
    r->hl_on = false;
    if (r->hl != NULL && r->color) {
        if (r->spans == NULL) {
            r->spans_cap = 256;
            r->spans = malloc((size_t)r->spans_cap * sizeof *r->spans);
        }
        if (r->spans != NULL) {
            r->nspans = mat_hl_line(r->hl, d, len, r->spans, r->spans_cap);
            r->hl_on = true;
        }
    }

    expand_tabs(r, d, len);

    /* Remap span offsets from original-byte to expanded-byte positions.
     * Must mirror expand_tabs exactly: decode UTF-8 and use mat_wcwidth. */
    if (r->hl_on && r->tab_width > 0 && memchr(d, '\t', len) != NULL) {
        int col = 0;
        size_t orig = 0, exp = 0;
        int si = 0;
        while (si < r->nspans && orig <= len) {
            struct mat_span *sp = &r->spans[si];
            while (orig < (size_t)sp->start && orig < len) {
                if (d[orig] == '\t') {
                    int tw = r->tab_width - (col % r->tab_width);
                    col += tw;
                    exp += (size_t)tw;
                    orig++;
                } else if (d[orig] < 0x80) {
                    col++;
                    exp++;
                    orig++;
                } else {
                    uint32_t cp;
                    size_t cl = mat_utf8_decode(d + orig, d + len, &cp);
                    col += mat_wcwidth(cp);
                    exp += cl;
                    orig += cl;
                }
            }
            unsigned new_start = (unsigned)exp;
            size_t sp_end = (size_t)sp->start + sp->len;
            while (orig < sp_end && orig < len) {
                if (d[orig] == '\t') {
                    int tw = r->tab_width - (col % r->tab_width);
                    col += tw;
                    exp += (size_t)tw;
                    orig++;
                } else if (d[orig] < 0x80) {
                    col++;
                    exp++;
                    orig++;
                } else {
                    uint32_t cp;
                    size_t cl = mat_utf8_decode(d + orig, d + len, &cp);
                    col += mat_wcwidth(cp);
                    exp += cl;
                    orig += cl;
                }
            }
            sp->start = new_start;
            sp->len = (unsigned)(exp - new_start);
            si++;
        }
    }

    const unsigned char *w = (const unsigned char *)r->wbuf;
    size_t wl = r->wbuf_len;
    int content_width = mat_render_content_width(r, width);

    if (r->wrap == MAT_WRAP_NEVER) {
        emit_seg(r, lineno, false, r->wbuf, wl, 0, sink, ctx);
        return 1;
    }

    bool word = (r->wrap == MAT_WRAP_WORD);
    size_t seg = 0, i = 0, last_ws = 0;
    bool have_ws = false, first = true;
    int col = 0, count = 0;
    while (i < wl) {
        uint32_t cp;
        size_t cl = mat_utf8_decode(w + i, w + wl, &cp);
        int cw = mat_wcwidth(cp);
        if (col + cw > content_width && i > seg) {
            size_t brk = i, next = i;
            if (word && have_ws && last_ws > seg) {
                brk = last_ws;
                next = last_ws + 1;
            }
            emit_seg(r, lineno, !first, r->wbuf + seg, brk - seg, seg, sink,
                     ctx);
            count++;
            first = false;
            seg = next;
            col = run_width(w + next, w + i);
            have_ws = false;
        }
        if (cp == ' ') {
            last_ws = i;
            have_ws = true;
        }
        col += cw;
        i += cl;
    }
    emit_seg(r, lineno, !first, r->wbuf + seg, wl - seg, seg, sink, ctx);
    return count + 1;
}
