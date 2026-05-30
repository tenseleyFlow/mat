#include "expand.h"

/*
 * The mapping, matching coreutils' quoting loop exactly:
 *   32..126   -> the byte itself (printable ASCII)
 *   127       -> "^?"
 *   >=128     -> "M-" + recurse on (byte & 0x7f):
 *                  160..254 -> that low byte (printable)
 *                  255      -> "^?"
 *                  128..159 -> "^" + (low - 128 + 64), i.e. M-^<ctrl>
 *   '\t'      -> literal tab when !show_tabs, else "^I" (falls to the ctrl
 * case)
 *   '\n'      -> itself (driver handles newlines)
 *   other <32 -> "^" + (byte + 64)
 */
int mat_expand_byte(unsigned char ch, bool show_tabs, unsigned char out[4])
{
    if (ch >= 32) {
        if (ch < 127) {
            out[0] = ch;
            return 1;
        }
        if (ch == 127) {
            out[0] = '^';
            out[1] = '?';
            return 2;
        }
        out[0] = 'M';
        out[1] = '-';
        if (ch >= 128 + 32) {
            if (ch < 255) {
                out[2] = (unsigned char)(ch - 128);
                return 3;
            }
            out[2] = '^';
            out[3] = '?';
            return 4;
        }
        out[2] = '^';
        out[3] = (unsigned char)(ch - 128 + 64);
        return 4;
    }
    if (ch == '\t' && !show_tabs) {
        out[0] = '\t';
        return 1;
    }
    if (ch == '\n') {
        out[0] = '\n';
        return 1;
    }
    out[0] = '^';
    out[1] = (unsigned char)(ch + 64);
    return 2;
}

void mat_xtable_build(struct mat_xtable *t, bool show_tabs)
{
    for (int i = 0; i < 256; i++)
        t->len[i] = (unsigned char)mat_expand_byte((unsigned char)i, show_tabs,
                                                   t->buf[i]);
}
