/*
 * expand.h — `cat -v` byte expansion (the quoting path).
 *
 * In quoting mode (-v, implied by -e and -t), every non-newline byte maps to a
 * fixed output sequence that depends only on the byte and whether tabs are
 * shown. We precompute a 256-entry table once per run so the hot loop is a
 * table lookup + memcpy, never a branch chain (and it pairs cleanly with SIMD
 * scanning in Sprint 02B). Byte semantics are byte-oriented like GNU cat — no
 * locale/wide-char involvement.
 */
#ifndef MAT_EXPAND_H
#define MAT_EXPAND_H

#include <stdbool.h>

struct mat_xtable {
    unsigned char len[256];    /* expansion length, 1..4 */
    unsigned char buf[256][4]; /* expansion bytes */
};

/*
 * Expand one byte into out[0..3] per cat -v rules; returns the length (1..4).
 * Newline (0x0A) maps to itself — the driver handles line boundaries.
 */
int mat_expand_byte(unsigned char ch, bool show_tabs, unsigned char out[4]);

/* Build the full 256-entry table for the given tab mode. */
void mat_xtable_build(struct mat_xtable *t, bool show_tabs);

#endif /* MAT_EXPAND_H */
