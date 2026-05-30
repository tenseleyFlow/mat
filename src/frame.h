/*
 * frame.h — decoration-frame chrome (grid rules + header lines).
 *
 * Shared by the streaming decorated printer (interactive.c) and the ranged
 * printer (rangeprint.c) so both draw an identical bat-style frame. Each
 * function emits one complete line (including the trailing newline) through a
 * sink, taking the gutter geometry from the shared struct mat_render.
 */
#ifndef MAT_FRAME_H
#define MAT_FRAME_H

#include "render.h" /* mat_sink_fn, struct mat_render, BX_*, COL_* */

/* A horizontal rule across `term_width`; `junction` is the glyph at the gutter
 * boundary (BX_D top, BX_X header split, BX_U bottom, BX_H plain join). */
void mat_frame_hrule(const struct mat_render *r, int term_width,
                     const char *junction, mat_sink_fn emit, void *ctx);

/* A "<label><value>" line prefixed by the empty gutter + vertical bar. */
void mat_frame_header_line(const struct mat_render *r, const char *label,
                           const char *value, mat_sink_fn emit, void *ctx);

#endif /* MAT_FRAME_H */
