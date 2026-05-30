/*
 * interactive.h — the decorated (framed) printer.
 *
 * Reached only when decorations are explicitly requested (--style /
 * --decorations=always / --pretty). Draws bat's frame: a line-number gutter,
 * the grid (│ / ─ / ┬┼┴), and the "File:" header. No syntax highlighting yet
 * (Sprint 08). The fast and cooked paths never enter here.
 *
 * mat's decoration set is small and fixed, so the gutter is rendered directly
 * rather than through a decoration vtable.
 */
#ifndef MAT_INTERACTIVE_H
#define MAT_INTERACTIVE_H

#include "config.h"

void mat_interactive_run(const struct config *cfg);

#endif /* MAT_INTERACTIVE_H */
