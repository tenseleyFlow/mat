/*
 * width.h — UTF-8 decoding and terminal display width.
 *
 * Used by the decoration path to wrap lines and expand tabs by display
 * columns (not bytes). The width table is an approximation of wcwidth covering
 * the common cases: ASCII, combining marks (0), and CJK/fullwidth/emoji (2).
 */
#ifndef MAT_WIDTH_H
#define MAT_WIDTH_H

#include <stddef.h>
#include <stdint.h>

/*
 * Decode one UTF-8 scalar starting at p (p < end), writing the codepoint to
 * *cp. Returns the number of bytes consumed (1 for ASCII or for an invalid /
 * truncated sequence, which is reported as its first byte).
 */
size_t mat_utf8_decode(const unsigned char *p, const unsigned char *end,
                       uint32_t *cp);

/* Display columns for a codepoint: 0 (combining), 1, or 2 (wide). */
int mat_wcwidth(uint32_t cp);

#endif /* MAT_WIDTH_H */
