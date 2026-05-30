/*
 * parallel.h — multi-file parallel decorated output.
 *
 * When decorations are on and there are multiple files, each file is
 * read+highlighted+rendered in a worker thread, then the results are written
 * to stdout in input order. Threshold-gated: single-file and tiny invocations
 * stay on the existing single-threaded path with zero thread overhead.
 */
#ifndef MAT_PARALLEL_H
#define MAT_PARALLEL_H

#include "config.h"

/* Run the decorated output for cfg's files in parallel when it pays off;
 * fall back to single-threaded otherwise. Returns true if it handled the
 * output (caller should not proceed to the single-threaded path). */
bool mat_parallel_run(const struct config *cfg);

#endif /* MAT_PARALLEL_H */
