/*
 * fastpath.h — the no-transform copy path.
 *
 * Sprint 00: a robust read()/full_write() loop with an adaptive, reused
 * buffer. Sprint 01 replaces the inner copy with the zero-copy ladder
 * (copy_file_range -> splice -> read/write) behind the same entry point.
 */
#ifndef MAT_FASTPATH_H
#define MAT_FASTPATH_H

#include <stdbool.h>
#include <sys/types.h>

#include "config.h"

/*
 * Concatenate every input in cfg to stdout with no transformation.
 * Recoverable errors are recorded via err.c; a stdout write failure is fatal
 * and stops processing. The process exit status is read from mat_status().
 */
void mat_fastpath_run(const struct config *cfg);

/*
 * Pure method-eligibility predicates — the gating matrix of the zero-copy
 * ladder, factored out so it can be unit-tested in isolation. These are always
 * compiled regardless of HAVE_* so the test suite is platform-independent.
 */

/* copy_file_range is for regular file -> regular file only. */
bool mat_cfr_eligible(bool in_isreg, bool out_isreg, bool cfr_ok);

/* Direct splice is for (any input) -> a pipe, skipping tiny regular files for
 * which read()/write() is faster. */
bool mat_splice_eligible(bool in_isreg, off_t in_size, bool out_ispipe,
                         bool splice_ok);

/* True if a copy_file_range errno means "unsupported here, fall back" rather
 * than a genuine I/O failure. */
bool mat_cfr_fallback_errno(int e);

#endif /* MAT_FASTPATH_H */
