#include "linesrc.h"
#include "encoding.h"
#include "err.h"
#include "scan.h"

#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define MAT_SLURP_MAX ((size_t)(256 * 1024 * 1024))

static _Thread_local sigjmp_buf sigbus_jmp;
static _Thread_local volatile sig_atomic_t sigbus_armed;
static _Thread_local void *sigbus_leak; /* freed on SIGBUS recovery */

static void on_sigbus(int sig)
{
    (void)sig;
    if (sigbus_armed)
        siglongjmp(sigbus_jmp, 1);
    _exit(128 + SIGBUS);
}

void mat_linesrc_install_sigbus(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigbus;
    sigaction(SIGBUS, &sa, NULL);
}

static void off_push(struct mat_linesrc *s, size_t v)
{
    if (s->noff == s->off_cap) {
        size_t nc = s->off_cap ? s->off_cap * 2 : 1024;
        size_t *nb = realloc(s->off, nc * sizeof *nb);
        if (nb == NULL) {
            s->eof_known = true;
            s->total = s->noff > 0 ? s->noff - 1 : 0;
            return;
        }
        s->off = nb;
        s->off_cap = nc;
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
    if (s->map && s->map_size)
        munmap(s->map, s->map_size);
    free(s->owned);
    free(s->off);
}

/* Read a non-seekable input (stdin/pipe) fully into memory. */
static char *slurp_fd(int fd, size_t *out)
{
    size_t cap = 1 << 16, len = 0;
    char *buf = malloc(cap);
    for (;;) {
        if (len == cap) {
            if (cap >= MAT_SLURP_MAX) {
                mat_warnx("input exceeds 256 MiB limit; truncating");
                break;
            }
            size_t nc = cap * 2;
            if (nc > MAT_SLURP_MAX)
                nc = MAT_SLURP_MAX;
            char *nb = realloc(buf, nc);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
            cap = nc;
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

/* Decode UTF-16 (after its BOM) into a fresh UTF-8 buffer. Lone/invalid
 * surrogates become U+FFFD. */
static char *decode_utf16(const unsigned char *p, size_t n, bool le,
                          size_t *outlen)
{
    if (n > SIZE_MAX / 2)
        return NULL;
    size_t cap = n + n / 2 + 16, len = 0;
    char *out = malloc(cap);
    sigbus_leak = out;
    if (out == NULL)
        return NULL;
    size_t i = 0;
    while (i + 1 < n) {
        uint32_t u = le ? (uint32_t)(p[i] | (p[i + 1] << 8))
                        : (uint32_t)((p[i] << 8) | p[i + 1]);
        i += 2;
        uint32_t cp;
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n) {
            uint32_t lo = le ? (uint32_t)(p[i] | (p[i + 1] << 8))
                             : (uint32_t)((p[i] << 8) | p[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                i += 2;
                cp = 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u);
            } else {
                cp = 0xFFFDu;
            }
        } else if (u >= 0xD800 && u <= 0xDFFF) {
            cp = 0xFFFDu; /* lone surrogate */
        } else {
            cp = u;
        }
        if (len + 4 > cap) {
            cap *= 2;
            char *nb = realloc(out, cap);
            if (nb == NULL) {
                free(out);
                return NULL;
            }
            out = nb;
        }
        if (cp < 0x80) {
            out[len++] = (char)cp;
        } else if (cp < 0x800) {
            out[len++] = (char)(0xC0u | (cp >> 6));
            out[len++] = (char)(0x80u | (cp & 0x3Fu));
        } else if (cp < 0x10000) {
            out[len++] = (char)(0xE0u | (cp >> 12));
            out[len++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
            out[len++] = (char)(0x80u | (cp & 0x3Fu));
        } else {
            out[len++] = (char)(0xF0u | (cp >> 18));
            out[len++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
            out[len++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
            out[len++] = (char)(0x80u | (cp & 0x3Fu));
        }
    }
    *outlen = len;
    return out;
}

/* Sniff the raw bytes and set up the logical (UTF-8) view: decode UTF-16, skip
 * a UTF-8 BOM, or leave the bytes as-is. The raw allocation (map or owned) is
 * recorded so it is released at close. */
static void set_content(struct mat_linesrc *s, const unsigned char *raw,
                        size_t rawlen)
{
    s->encoding = mat_encoding_sniff(raw, rawlen);
    size_t bom = mat_encoding_bom_len(s->encoding, raw, rawlen);

    if (s->encoding == MAT_ENC_UTF16LE || s->encoding == MAT_ENC_UTF16BE) {
        size_t declen = 0;
        char *dec = decode_utf16(raw + bom, rawlen - bom,
                                 s->encoding == MAT_ENC_UTF16LE, &declen);
        if (dec != NULL) {
            if (s->map && s->map_size) {
                munmap(s->map, s->map_size);
                s->map = NULL;
                s->map_size = 0;
            }
            free(s->owned);
            s->owned = dec;
            s->data = dec;
            s->size = declen;
            return;
        }
        /* decode failed: fall through and show the raw bytes */
    }
    s->data = (const char *)raw + bom;
    s->size = rawlen - bom;
}

bool mat_linesrc_open(struct mat_linesrc *s, int fd)
{
    memset(s, 0, sizeof *s);
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
        void *m = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) {
            sigbus_leak = NULL;
            sigbus_armed = 1;
            if (sigsetjmp(sigbus_jmp, 1) != 0) {
                sigbus_armed = 0;
                free(sigbus_leak);
                sigbus_leak = NULL;
                s->map = NULL;
                s->map_size = 0;
                munmap(m, (size_t)st.st_size);
                mat_warnx("file truncated during read");
                /* fall through to slurp path */
            } else {
                s->map = m;
                s->map_size = (size_t)st.st_size;
                set_content(s, m, (size_t)st.st_size);
                off_push(s, 0);
                sigbus_armed = 0;
                sigbus_leak = NULL;
                return true;
            }
        }
    }
    size_t len = 0;
    char *buf = slurp_fd(fd, &len);
    if (buf == NULL)
        return false;
    s->owned = buf;
    set_content(s, (const unsigned char *)buf, len);
    off_push(s, 0);
    return true;
}
