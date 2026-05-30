/*
 * compat.h — portability shims and branch hints.
 *
 * The one place that hides differences between FreeBSD, Linux, and macOS.
 * Sprint 00 keeps this small; the syscall capability macros
 * (HAVE_COPY_FILE_RANGE, ...) arrive in Sprint 01 via configure probes and are
 * included through config_generated.h.
 */
#ifndef MAT_COMPAT_H
#define MAT_COMPAT_H

#include "config_generated.h"

/* Branch hints for the hot path. */
#if defined(__GNUC__) || defined(__clang__)
#define MAT_LIKELY(x)   __builtin_expect(!!(x), 1)
#define MAT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define MAT_LIKELY(x)   (x)
#define MAT_UNLIKELY(x) (x)
#endif

/* C11 restrict spelling that survives -std=c11 -pedantic. */
#if defined(__GNUC__) || defined(__clang__)
#define MAT_RESTRICT __restrict__
#else
#define MAT_RESTRICT
#endif

#endif /* MAT_COMPAT_H */
