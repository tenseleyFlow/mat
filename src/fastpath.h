/*
 * fastpath.h — the no-transform copy path.
 *
 * Sprint 00: a robust read()/full_write() loop with an adaptive, reused
 * buffer. Sprint 01 replaces the inner copy with the zero-copy ladder
 * (copy_file_range -> splice -> read/write) behind the same entry point.
 */
#ifndef MAT_FASTPATH_H
#define MAT_FASTPATH_H

#include "config.h"

/*
 * Concatenate every input in cfg to stdout with no transformation.
 * Recoverable errors are recorded via err.c; a stdout write failure is fatal
 * and stops processing. The process exit status is read from mat_status().
 */
void mat_fastpath_run(const struct config *cfg);

#endif /* MAT_FASTPATH_H */
