/*
 * rangeprint.h — print only selected line ranges (-r) of the input.
 *
 * Runs on the shared line-indexed source (linesrc.c), so the same line model
 * backs both this non-paged printer and the pager. Plain (raw) output: each
 * selected line as-is, followed by a newline. Decoration of ranged output is
 * layered on separately.
 */
#ifndef MAT_RANGEPRINT_H
#define MAT_RANGEPRINT_H

#include "config.h"

/* Print the lines selected by cfg->ranges across all inputs to stdout. */
void mat_rangeprint_run(const struct config *cfg);

#endif /* MAT_RANGEPRINT_H */
