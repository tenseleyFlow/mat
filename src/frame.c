#include "frame.h"

#include <string.h>

static void e_str(mat_sink_fn emit, void *ctx, const char *s)
{
    emit(ctx, s, strlen(s));
}

static void e_rep(mat_sink_fn emit, void *ctx, const char *s, int n)
{
    size_t slen = strlen(s);
    size_t total = slen * (size_t)n;
    char buf[1024];
    if (total <= sizeof buf) {
        for (int i = 0; i < n; i++)
            memcpy(buf + (size_t)i * slen, s, slen);
        emit(ctx, buf, total);
    } else {
        for (int i = 0; i < n; i++)
            emit(ctx, s, slen);
    }
}

void mat_frame_hrule(const struct mat_render *r, int term_width,
                     const char *junction, mat_sink_fn emit, void *ctx)
{
    if (r->color)
        e_str(emit, ctx, COL_GUTTER);
    if (r->panel_width > 0) {
        e_rep(emit, ctx, BX_H, r->panel_width);
        e_str(emit, ctx, junction);
        int rest = term_width - r->panel_width - 1;
        if (rest > 0)
            e_rep(emit, ctx, BX_H, rest);
    } else {
        e_rep(emit, ctx, BX_H, term_width);
    }
    if (r->color)
        e_str(emit, ctx, COL_RESET);
    e_str(emit, ctx, "\n");
}

void mat_frame_header_line(const struct mat_render *r, const char *label,
                           const char *value, mat_sink_fn emit, void *ctx)
{
    if (r->panel_width > 0) {
        if (r->color)
            e_str(emit, ctx, COL_GUTTER);
        e_rep(emit, ctx, " ", r->panel_width);
        if (r->grid)
            e_str(emit, ctx, BX_V " ");
        if (r->color)
            e_str(emit, ctx, COL_RESET);
    }
    e_str(emit, ctx, label);
    e_str(emit, ctx, value);
    e_str(emit, ctx, "\n");
}
