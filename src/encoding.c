#include "encoding.h"

#include <string.h>

enum mat_encoding mat_encoding_sniff(const unsigned char *buf, size_t len)
{
    if (len == 0)
        return MAT_ENC_EMPTY;

    /* UTF-32 BOMs first: they begin with the UTF-16 BOM bytes, so check the
     * longer pattern before falling through. We don't decode UTF-32, so it is
     * reported as binary. */
    if (len >= 4 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0xFE &&
        buf[3] == 0xFF)
        return MAT_ENC_BINARY; /* UTF-32BE */
    if (len >= 4 && buf[0] == 0xFF && buf[1] == 0xFE && buf[2] == 0x00 &&
        buf[3] == 0x00)
        return MAT_ENC_BINARY; /* UTF-32LE */

    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF)
        return MAT_ENC_UTF8; /* UTF-8 BOM */
    if (len >= 2 && buf[0] == 0xFF && buf[1] == 0xFE)
        return MAT_ENC_UTF16LE;
    if (len >= 2 && buf[0] == 0xFE && buf[1] == 0xFF)
        return MAT_ENC_UTF16BE;

    /* No BOM: a NUL byte means binary (BOM-less UTF-16 is not distinguished
     * from binary here — it is rare and would need a parity heuristic). */
    if (memchr(buf, 0, len) != NULL)
        return MAT_ENC_BINARY;

    return MAT_ENC_UTF8;
}

size_t mat_encoding_bom_len(enum mat_encoding enc, const unsigned char *buf,
                            size_t len)
{
    if (enc == MAT_ENC_UTF8 && len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB &&
        buf[2] == 0xBF)
        return 3;
    if ((enc == MAT_ENC_UTF16LE || enc == MAT_ENC_UTF16BE) && len >= 2)
        return 2; /* we only detect UTF-16 via its BOM */
    return 0;
}

const char *mat_encoding_name(enum mat_encoding enc)
{
    switch (enc) {
    case MAT_ENC_EMPTY:
        return "empty";
    case MAT_ENC_UTF8:
        return "UTF-8";
    case MAT_ENC_UTF16LE:
        return "UTF-16LE";
    case MAT_ENC_UTF16BE:
        return "UTF-16BE";
    case MAT_ENC_BINARY:
        return "binary";
    }
    return "unknown";
}
