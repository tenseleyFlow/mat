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
}

void mat_render_free(struct mat_render *r)
{
    free(r->wbuf);
    free(r->seg);
}

int mat_render_content_width(const struct mat_render *r, int width)
{
    int gutter_vis = r->numbers ? (r->panel_width + (r->grid ? 2 : 0)) : 0;
    int cw = width - gutter_vis;
    return cw < 1 ? 1 : cw;
}

/* ---- assembly buffers ---- */

static void wbuf_append(struct mat_render *r, const char *d, size_t n)
{
    if (r->wbuf_len + n > r->wbuf_cap) {
        size_t cap = r->wbuf_cap ? r->wbuf_cap * 2 : 8192;
        while (cap < r->wbuf_len + n)
            cap *= 2;
        char *nb = realloc(r->wbuf, cap);
        if (!nb)
            return;
        r->wbuf = nb;
        r->wbuf_cap = cap;
    }
    memcpy(r->wbuf + r->wbuf_len, d, n);
    r->wbuf_len += n;
}

static void seg_append(struct mat_render *r, const char *d, size_t n)
{
    if (r->seg_len + n > r->seg_cap) {
        size_t cap = r->seg_cap ? r->seg_cap * 2 : 256;
        while (cap < r->seg_len + n)
            cap *= 2;
        char *nb = realloc(r->seg, cap);
        if (!nb)
            return;
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
    if (r->tab_width <= 0) {
        wbuf_append(r, (const char *)d, len);
        return;
    }
    int col = 0;
    size_t i = 0;
    while (i < len) {
        if (d[i] == '\t') {
            int sp = r->tab_width - (col % r->tab_width);
            for (int k = 0; k < sp; k++)
                wbuf_append(r, " ", 1);
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
            char num[32];
            int len = snprintf(num, sizeof num, "%4lu ", n);
            if (len > 0)
                seg_append(r, num, (size_t)len);
        }
    }
    if (r->grid && r->panel_width > 0)
        seg_str(r, BX_V " ");
    if (r->color)
        seg_str(r, COL_RESET);
}

static void emit_seg(struct mat_render *r, unsigned long n, bool continuation,
                     const char *content, size_t clen, mat_sink_fn sink,
                     void *ctx)
{
    r->seg_len = 0;
    put_gutter(r, n, continuation);
    seg_append(r, content, clen);
    sink(ctx, r->seg, r->seg_len);
}

int mat_render_line(struct mat_render *r, unsigned long lineno,
                    const unsigned char *d, size_t len, int width,
                    mat_sink_fn sink, void *ctx)
{
    expand_tabs(r, d, len);
    const unsigned char *w = (const unsigned char *)r->wbuf;
    size_t wl = r->wbuf_len;
    int content_width = mat_render_content_width(r, width);

    if (r->wrap == MAT_WRAP_NEVER) {
        emit_seg(r, lineno, false, r->wbuf, wl, sink, ctx);
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
            emit_seg(r, lineno, !first, r->wbuf + seg, brk - seg, sink, ctx);
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
    emit_seg(r, lineno, !first, r->wbuf + seg, wl - seg, sink, ctx);
    return count + 1;
}
