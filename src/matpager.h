/*
 * matpager.h — glue between mat's renderer and the paige pager engine.
 *
 * Pages a single input lazily: a regular file is mmap'd (so opening a multi-GB
 * file is instant) and its line offsets are indexed only as far as you scroll.
 * Each visible line is rendered through render.c into paige's segments.
 */
#ifndef MAT_PAGER_H
#define MAT_PAGER_H

#include <stdbool.h>

#include "config.h"

/*
 * Page the single input (cfg->files[0], or stdin if none). `decorated` selects
 * the gutter style. Returns 0 on success/quit, or -1 if there is no terminal
 * (the caller should then fall back to streaming output).
 */
int mat_page(const struct config *cfg, bool decorated);

#endif /* MAT_PAGER_H */
