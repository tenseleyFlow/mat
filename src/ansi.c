#include "ansi.h"

size_t mat_strip_ansi(const unsigned char *in, size_t n, char *out)
{
    size_t o = 0;
    size_t i = 0;
    while (i < n) {
        if (in[i] != 0x1b) { /* not an escape */
            out[o++] = (char)in[i++];
            continue;
        }
        if (i + 1 >= n) {
            i++; /* lone trailing ESC: drop it */
            continue;
        }
        unsigned char c = in[i + 1];
        if (c == '[') {
            /* CSI: ESC [ params(0x30-0x3F) intermediates(0x20-0x2F) final */
            i += 2;
            while (i < n && in[i] >= 0x20 && in[i] <= 0x3F)
                i++;
            if (i < n && in[i] >= 0x40 && in[i] <= 0x7E)
                i++;
        } else if (c == ']') {
            /* OSC: ESC ] ... terminated by BEL or ESC \ */
            i += 2;
            while (i < n) {
                if (in[i] == 0x07) {
                    i++;
                    break;
                }
                if (in[i] == 0x1b && i + 1 < n && in[i + 1] == '\\') {
                    i += 2;
                    break;
                }
                i++;
            }
        } else {
            i += 2; /* other two-byte ESC sequence */
        }
    }
    return o;
}
