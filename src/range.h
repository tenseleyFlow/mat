/*
 * range.h — line-range selection (-r/--line-range) and the shared grammar used
 * by -H/--highlight-line.
 *
 * A range set accumulates one or more range expressions (multiple -r flags) and
 * answers "is line N selected?" against a known total line count. Most forms
 * are absolute and need no total; the relative forms (-N: "last N") need EOF to
 * be known, which is what drives the lookahead ring in the cooked/interactive
 * printers. Pure data + pure functions: no I/O, no allocation, fast-path-free.
 */
#ifndef MAT_RANGE_H
#define MAT_RANGE_H

#include <stdbool.h>
#include <stddef.h>

#define MAT_RANGES_MAX 64

struct mat_range {
    long lo;     /* 1-based start; when lo_rel, the N in "last N" */
    long hi;     /* 1-based inclusive end; valid unless hi_inf */
    bool lo_rel; /* start counts back from the last line (the -N: form) */
    bool hi_inf; /* range extends to the last line (N:, -N:) */
};

struct mat_rangeset {
    struct mat_range r[MAT_RANGES_MAX];
    int n;
    bool needs_total; /* any relative bound: the caller must know EOF/total */
    long max_tail;    /* largest "last N" => lookahead ring size (0 if none) */
};

void mat_rangeset_init(struct mat_rangeset *rs);

/*
 * Parse one range expression (one -r/-H argument) and append it. Forms:
 *   N        single line N
 *   N:M      lines N..M inclusive
 *   :M       lines 1..M
 *   N:       line N to the end
 *   -N:      the last N lines
 *   N:+M     line N plus M more (N..N+M)
 *   N::C     line N with C lines of context (N-C..N+C)
 * Returns 0 on success; on error returns -1 and writes a message into err
 * (truncated to errn). The set is left unchanged on error.
 */
int mat_range_parse(struct mat_rangeset *rs, const char *arg, char *err,
                    size_t errn);

/*
 * True if 1-based line `line` is selected given the document's `total` line
 * count. `total` is consulted only for relative/open-ended bounds. An empty set
 * selects every line, so callers can always consult it uniformly.
 */
bool mat_rangeset_contains(const struct mat_rangeset *rs, long line,
                           long total);

/*
 * Highest absolute line the set can ever select, or LONG_MAX if it is
 * open-ended (N:, -N:) and must be read to EOF. An empty set is unbounded.
 * Lets a bounded selection stop reading early.
 */
long mat_rangeset_max_line(const struct mat_rangeset *rs);

#endif /* MAT_RANGE_H */
