/*
 * render.h — render one logical line into visual segments.
 *
 * Shared by the streaming decorated printer (interactive.c) and the pager glue
 * (matpager.c): both need the gutter + tab-expansion + wrapping, differing only
 * in where the finished segments go. The renderer emits each visual segment
 * (gutter + content, no newline) through a sink callback.
 */
#ifndef MAT_RENDER_H
#define MAT_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

/* Box-drawing glyphs (explicit UTF-8) and gutter color, shared with the frame
 * chrome in interactive.c. */
#define BX_H "\xe2\x94\x80" /* ─ */
#define BX_V "\xe2\x94\x82" /* │ */
#define BX_D "\xe2\x94\xac" /* ┬ */
#define BX_X "\xe2\x94\xbc" /* ┼ */
#define BX_U "\xe2\x94\xb4" /* ┴ */
#define COL_GUTTER "\x1b[38;5;238m"
#define COL_RESET "\x1b[0m"

struct mat_render {
    bool numbers, grid, color;
    int panel_width; /* 5 if numbers else 0 */
    int tab_width;   /* 0 = no tab expansion */
    enum mat_wrap wrap;
    char *wbuf; /* tab-expanded line, reused */
    size_t wbuf_cap, wbuf_len;
    char *seg; /* one assembled segment, reused */
    size_t seg_cap, seg_len;
};

void mat_render_init(struct mat_render *r, unsigned style, enum mat_wrap wrap,
                     int tab_width, bool color);
void mat_render_free(struct mat_render *r);

/* Number of content columns available after the gutter at total width `width`.
 */
int mat_render_content_width(const struct mat_render *r, int width);

/* Called once per visual segment (gutter + content bytes; no trailing newline).
 */
typedef void (*mat_sink_fn)(void *ctx, const char *bytes, size_t len);

/*
 * Render logical line `lineno` (1-based, for the gutter) with content [d,len)
 * laid out for a total line width of `width` columns. Returns the number of
 * visual segments emitted.
 */
int mat_render_line(struct mat_render *r, unsigned long lineno,
                    const unsigned char *d, size_t len, int width,
                    mat_sink_fn sink, void *ctx);

#endif /* MAT_RENDER_H */
