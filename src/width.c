#include "width.h"

size_t mat_utf8_decode(const unsigned char *p, const unsigned char *end,
                       uint32_t *cp)
{
    unsigned char c = p[0];
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    int len;
    uint32_t v;
    if ((c & 0xE0) == 0xC0) {
        len = 2;
        v = c & 0x1Fu;
    } else if ((c & 0xF0) == 0xE0) {
        len = 3;
        v = c & 0x0Fu;
    } else if ((c & 0xF8) == 0xF0) {
        len = 4;
        v = c & 0x07u;
    } else {
        *cp = c; /* invalid lead byte */
        return 1;
    }
    if (p + len > end) {
        *cp = c; /* truncated */
        return 1;
    }
    for (int i = 1; i < len; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            *cp = c; /* invalid continuation */
            return 1;
        }
        v = (v << 6) | (uint32_t)(p[i] & 0x3Fu);
    }
    *cp = v;
    return (size_t)len;
}

int mat_wcwidth(uint32_t c)
{
    if (c == 0)
        return 0;
    /* Control characters: treated as a single cell (rare in viewed text). */
    if (c < 0x20 || (c >= 0x7F && c < 0xA0))
        return 1;
    /* Zero-width combining marks. */
    if ((c >= 0x0300 && c <= 0x036F) || (c >= 0x0483 && c <= 0x0489) ||
        (c >= 0x1AB0 && c <= 0x1AFF) || (c >= 0x1DC0 && c <= 0x1DFF) ||
        (c >= 0x20D0 && c <= 0x20FF) || (c >= 0xFE20 && c <= 0xFE2F) ||
        c == 0x200B /* ZWSP */)
        return 0;
    /* Wide (East-Asian fullwidth / wide) and emoji. */
    if ((c >= 0x1100 && c <= 0x115F) || c == 0x2329 || c == 0x232A ||
        (c >= 0x2E80 && c <= 0x303E) || (c >= 0x3041 && c <= 0x33FF) ||
        (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) ||
        (c >= 0xA000 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE10 && c <= 0xFE19) ||
        (c >= 0xFE30 && c <= 0xFE6F) || (c >= 0xFF00 && c <= 0xFF60) ||
        (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1FAFF) ||
        (c >= 0x20000 && c <= 0x3FFFD))
        return 2;
    return 1;
}
