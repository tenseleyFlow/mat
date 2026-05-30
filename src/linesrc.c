#include "linesrc.h"
#include "scan.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

static void off_push(struct mat_linesrc *s, size_t v)
{
    if (s->noff == s->off_cap) {
        s->off_cap = s->off_cap ? s->off_cap * 2 : 1024;
        s->off = realloc(s->off, s->off_cap * sizeof *s->off);
    }
    s->off[s->noff++] = v;
}

/* Ensure line offsets are known through index `want` (or to EOF). */
static void ensure(struct mat_linesrc *s, size_t want)
{
    while (!s->eof_known && s->noff <= want) {
        size_t from = s->off[s->noff - 1];
        if (from >= s->size) {
            s->eof_known = true;
            s->total = s->noff - 1; /* off[noff-1]==size: not a real line */
            return;
        }
        const unsigned char *base = (const unsigned char *)s->data;
        const unsigned char *q = mat_scan_newline(base + from, base + s->size);
        if (q == base + s->size) {
            s->eof_known = true;
            s->total =
                s->noff; /* final line [from, size) with no trailing nl */
            return;
        }
        size_t nl = (size_t)(q - base);
        if (nl + 1 < s->size) {
            off_push(s, nl + 1);
        } else {
            s->eof_known = true;
            s->total = s->noff; /* trailing newline: line `noff-1` ends here */
            return;
        }
    }
}

bool mat_linesrc_line(struct mat_linesrc *s, size_t L, const unsigned char **d,
                      size_t *len)
{
    ensure(s, L + 1);
    if (s->eof_known && L >= s->total)
        return false;
    size_t start = s->off[L];
    size_t end;
    if (L + 1 < s->noff) {
        end = s->off[L + 1] - 1; /* drop the separating newline */
    } else {
        /* last line: trim a trailing newline if the file ended with one */
        end = s->size;
        if (end > start && s->data[end - 1] == '\n')
            end--;
    }
    *d = (const unsigned char *)s->data + start;
    *len = end > start ? end - start : 0;
    return true;
}

size_t mat_linesrc_total(struct mat_linesrc *s)
{
    ensure(s, (size_t)-1);
    return s->total;
}

void mat_linesrc_free(struct mat_linesrc *s)
{
    if (s->mmapped && s->data && s->size)
        munmap((void *)s->data, s->size);
    else
        free((void *)s->data);
    free(s->off);
}

/* Read a non-seekable input (stdin/pipe) fully into memory. */
static char *slurp_fd(int fd, size_t *out)
{
    size_t cap = 1 << 16, len = 0;
    char *buf = malloc(cap);
    for (;;) {
        if (len == cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        ssize_t r = read(fd, buf + len, cap - len);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            free(buf);
            return NULL;
        }
        if (r == 0)
            break;
        len += (size_t)r;
    }
    *out = len;
    return buf;
}

bool mat_linesrc_open(struct mat_linesrc *s, int fd)
{
    memset(s, 0, sizeof *s);
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
        void *m = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) {
            s->data = m;
            s->size = (size_t)st.st_size;
            s->mmapped = true;
        }
    }
    if (s->data == NULL) {
        s->data = slurp_fd(fd, &s->size);
        if (s->data == NULL)
            return false;
    }
    off_push(s, 0);
    return true;
}
