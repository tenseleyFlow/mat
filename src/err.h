/*
 * err.h — diagnostics and exit-status accumulation.
 *
 * cat's contract: read/open errors are recoverable (warn, keep going, remember
 * we failed); the process exits non-zero if anything failed. Write errors are
 * fatal and handled at the call site. We never call exit() from a warn path.
 */
#ifndef MAT_ERR_H
#define MAT_ERR_H

extern const char *mat_progname; /* set once in main from argv[0] */

/* Records failure (sets the sticky error flag) and prints "mat: <ctx>:
 * <strerror>". */
void mat_warn(const char *ctx);

/* Like mat_warn but the message is literal (no errno). */
void mat_warnx(const char *msg);

/* Marks that some operation failed, without printing. */
void mat_fail(void);

/* 0 if everything succeeded, 1 if any recoverable error was recorded. */
int mat_status(void);

#endif /* MAT_ERR_H */
