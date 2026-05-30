/*
 * encoding.h — leading-block content sniff for the decorated path.
 *
 * UTF-8 is the fall-through happy path (no work); UTF-16 is recognized by its
 * BOM and decoded only in an explicit branch; a NUL byte without a UTF-16 BOM
 * means binary. The fast and cooked (cat-compatible) paths never call this —
 * they stay byte-exact — so sniffing costs nothing unless decorations are on.
 */
#ifndef MAT_ENCODING_H
#define MAT_ENCODING_H

#include <stddef.h>

enum mat_encoding {
    MAT_ENC_EMPTY,   /* no bytes */
    MAT_ENC_UTF8,    /* UTF-8 / ASCII — the zero-overhead happy path */
    MAT_ENC_UTF16LE, /* recognized by a FF FE BOM */
    MAT_ENC_UTF16BE, /* recognized by a FE FF BOM */
    MAT_ENC_BINARY,  /* NUL bytes / unsupported (UTF-32, etc.) */
};

/* Sniff the encoding from a leading block (a few KB is plenty). */
enum mat_encoding mat_encoding_sniff(const unsigned char *buf, size_t len);

/* Bytes of byte-order mark to skip for `enc` (3 for a UTF-8 BOM, 2 for UTF-16,
 * 0 otherwise). `buf`/`len` are the same leading bytes passed to the sniff. */
size_t mat_encoding_bom_len(enum mat_encoding enc, const unsigned char *buf,
                            size_t len);

/* Human-readable name, for --detect-syntax / diagnostics. */
const char *mat_encoding_name(enum mat_encoding enc);

#endif /* MAT_ENCODING_H */
