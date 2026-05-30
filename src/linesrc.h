/*
 * linesrc.h — a read-only byte source with a lazily-built logical-line index.
 *
 * Opens a file (mmap) or drains a stream (slurp), then hands out any logical
 * line by number, building the newline offset index only as far as asked. The
 * total line count is known only once the index reaches EOF, so callers that
 * need it (last-N ranges, the pager's bottom) pay the full scan; callers that
 * page or print a bounded prefix never touch the tail. Shared by the pager
 * (matpager.c) and the line-range printer (rangeprint.c).
 */
#ifndef MAT_LINESRC_H
#define MAT_LINESRC_H

#include <stdbool.h>
#include <stddef.h>

#include "encoding.h"

struct mat_linesrc {
    /* Logical content the index/lines see: UTF-8 (decoded from UTF-16 when the
     * source was UTF-16; a leading BOM is skipped). */
    const char *data;
    size_t size;
    enum mat_encoding encoding; /* the detected source encoding */

    /* Ownership of the backing bytes, freed at close. */
    void *map; /* mmap base (munmap), or NULL */
    size_t map_size;
    void *owned; /* malloc'd buffer (free), or NULL */

    size_t *off; /* off[i] = byte offset of line i */
    size_t noff, off_cap;
    bool eof_known;
    size_t total; /* valid once eof_known */
};

/* mmap a regular file, else slurp the fd into memory. Returns false on a read
 * failure (errno set). */
bool mat_linesrc_open(struct mat_linesrc *s, int fd);

/* Fetch logical line L (0-based). Returns false at/after EOF. The bytes exclude
 * the trailing newline; they point into the source and stay valid until free.
 */
bool mat_linesrc_line(struct mat_linesrc *s, size_t L, const unsigned char **d,
                      size_t *len);

/* Total logical line count, forcing the index to EOF. */
size_t mat_linesrc_total(struct mat_linesrc *s);

void mat_linesrc_free(struct mat_linesrc *s);

#endif /* MAT_LINESRC_H */
