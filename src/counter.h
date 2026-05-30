/*
 * counter.h — pre-formatted decimal line counter (-n / -b).
 *
 * Mirrors coreutils' line_buf/next_line_num: a fixed buffer holding
 * "     N\t" that is incremented in place, so emitting a line number is a
 * memcpy of a ready-made string rather than a per-line printf. Right-justified
 * in a 6-wide field, widening only past 999999 (then a leading '>' on
 * overflow), byte-for-byte matching cat's output.
 */
#ifndef MAT_COUNTER_H
#define MAT_COUNTER_H

#define MAT_LINE_BUF_LEN 20

struct mat_counter {
    char buf[MAT_LINE_BUF_LEN];
    char *print; /* start of the printed field within buf */
    char *start; /* first digit */
    char *end;   /* last digit */
};

void mat_counter_init(struct mat_counter *c);

/* Advance to the next number and return a NUL-terminated "     N\t" string. */
const char *mat_counter_next(struct mat_counter *c);

#endif /* MAT_COUNTER_H */
