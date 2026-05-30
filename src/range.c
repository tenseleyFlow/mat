#include "range.h"

#include <limits.h>
#include <stdio.h>

#define MAT_RANGE_CAP 1000000000L /* clamp parsed values; avoids overflow */

void mat_rangeset_init(struct mat_rangeset *rs)
{
    rs->n = 0;
    rs->needs_total = false;
    rs->max_tail = 0;
}

/* Parse a run of decimal digits. Returns 0 and advances *end past the digits,
 * or -1 if there is no digit at *s. Values are clamped to MAT_RANGE_CAP. */
static int p_num(const char *s, const char **end, long *out)
{
    if (*s < '0' || *s > '9')
        return -1;
    long v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        if (v > MAT_RANGE_CAP)
            v = MAT_RANGE_CAP;
        s++;
    }
    *out = v;
    *end = s;
    return 0;
}

static int fail(char *err, size_t errn, const char *arg)
{
    if (err && errn)
        snprintf(err, errn, "invalid line range '%s'", arg);
    return -1;
}

int mat_range_parse(struct mat_rangeset *rs, const char *arg, char *err,
                    size_t errn)
{
    if (rs->n >= MAT_RANGES_MAX) {
        if (err && errn)
            snprintf(err, errn, "too many line ranges (max %d)",
                     MAT_RANGES_MAX);
        return -1;
    }

    struct mat_range out = {0, 0, false, false};
    const char *p = arg;
    const char *e;

    /* -N: — the last N lines. */
    if (p[0] == '-') {
        long n;
        if (p_num(p + 1, &e, &n) != 0 || n < 1)
            return fail(err, errn, arg);
        if (e[0] != ':' || e[1] != '\0')
            return fail(err, errn, arg);
        out.lo = n;
        out.lo_rel = true;
        out.hi_inf = true;
    } else {
        const char *colon = arg;
        while (*colon && *colon != ':')
            colon++;

        if (*colon == '\0') {
            /* N — a single line. */
            long n;
            if (p_num(arg, &e, &n) != 0 || *e != '\0' || n < 1)
                return fail(err, errn, arg);
            out.lo = n;
            out.hi = n;
        } else {
            /* Left of the first colon: empty means "from the start". */
            long lo = 1;
            bool have_lo = false;
            if (colon != arg) {
                if (p_num(arg, &e, &lo) != 0 || e != colon || lo < 1)
                    return fail(err, errn, arg);
                have_lo = true;
            }
            const char *rest = colon + 1;

            if (rest[0] == ':') {
                /* N::C — line N with C lines of context. */
                long c;
                if (!have_lo)
                    return fail(err, errn, arg);
                if (p_num(rest + 1, &e, &c) != 0 || *e != '\0')
                    return fail(err, errn, arg);
                out.lo = lo - c < 1 ? 1 : lo - c;
                out.hi = lo + c;
            } else if (rest[0] == '+') {
                /* N:+M — line N plus M more. */
                long m;
                if (!have_lo)
                    return fail(err, errn, arg);
                if (p_num(rest + 1, &e, &m) != 0 || *e != '\0')
                    return fail(err, errn, arg);
                out.lo = lo;
                out.hi = lo + m;
            } else if (rest[0] == '\0') {
                /* N: — to the end. */
                if (!have_lo)
                    return fail(err, errn, arg); /* ":" alone is meaningless */
                out.lo = lo;
                out.hi_inf = true;
            } else {
                /* N:M or :M. */
                long m;
                if (p_num(rest, &e, &m) != 0 || *e != '\0' || m < 1)
                    return fail(err, errn, arg);
                out.lo = lo;
                out.hi = m;
            }
        }
    }

    rs->r[rs->n++] = out;
    if (out.lo_rel) {
        rs->needs_total = true;
        if (out.lo > rs->max_tail)
            rs->max_tail = out.lo;
    }
    return 0;
}

bool mat_rangeset_contains(const struct mat_rangeset *rs, long line, long total)
{
    if (rs->n == 0)
        return true; /* no selection => everything */
    for (int i = 0; i < rs->n; i++) {
        const struct mat_range *r = &rs->r[i];
        long lo = r->lo;
        if (r->lo_rel) {
            lo = total - r->lo + 1; /* last N lines */
            if (lo < 1)
                lo = 1;
        }
        if (line < lo)
            continue;
        if (!r->hi_inf && line > r->hi)
            continue;
        return true;
    }
    return false;
}

long mat_rangeset_max_line(const struct mat_rangeset *rs)
{
    if (rs->n == 0)
        return LONG_MAX;
    long max = 0;
    for (int i = 0; i < rs->n; i++) {
        const struct mat_range *r = &rs->r[i];
        if (r->hi_inf || r->lo_rel)
            return LONG_MAX;
        if (r->hi > max)
            max = r->hi;
    }
    return max;
}
