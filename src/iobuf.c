#include "iobuf.h"
#include "compat.h"

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef HAVE_VMSPLICE
#include <fcntl.h>
#include <sys/uio.h>
#endif

/*
 * Bounds. We never go below one page, never above 1 MiB. The cap reflects
 * diminishing returns: past ~1 MiB the per-syscall amortization is flat while
 * peak RSS for many-file runs keeps growing (audit 01 §1).
 */
#define MAT_BUF_MIN ((size_t)4096)
#define MAT_BUF_CAP ((size_t)(1u << 20))
#define MAT_BUF_DEF ((size_t)(128u << 10))

static size_t page_size(void)
{
    static size_t cached;
    if (cached == 0) {
        long p = sysconf(_SC_PAGESIZE);
        cached = (p > 0) ? (size_t)p : MAT_BUF_MIN;
    }
    return cached;
}

static size_t clamp(size_t v, size_t lo, size_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

size_t mat_iobuf_size(int in_fd, int out_fd)
{
    struct stat st;
    size_t pg = page_size();
    size_t blk = MAT_BUF_DEF;
    int is_reg = 0;

    if (fstat(in_fd, &st) == 0) {
        if (st.st_blksize > 0)
            blk = (size_t)st.st_blksize;
        is_reg = S_ISREG(st.st_mode);
    }

    /*
     * Regular-file source: scale up toward the cap so big copies do fewer, fat
     * reads. The zero-copy ladder in Sprint 01 will often bypass this path
     * entirely, but it remains the fallback. out_fd is reserved for a future
     * same-filesystem heuristic; touch it to keep the signature honest.
     */
    (void)out_fd;
    if (is_reg) {
        size_t want = blk;
        if (want < MAT_BUF_DEF)
            want = MAT_BUF_DEF;
        return clamp(want, pg, MAT_BUF_CAP);
    }

    /* Pipes/devices: honor the hint, floored at a page. */
    return clamp(blk, pg, MAT_BUF_CAP);
}

int mat_full_write(int fd, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (w == 0) {
            errno = EIO; /* shouldn't happen on a blocking fd; guard anyway */
            return -1;
        }
        p += (size_t)w;
        n -= (size_t)w;
    }
    return 0;
}

int mat_pipe_write(int fd, const void *buf, size_t n)
{
#ifdef HAVE_VMSPLICE
    static int cached_fd = -1, cached_is_pipe = -1;
    int is_pipe;
    if (fd == cached_fd && cached_is_pipe >= 0) {
        is_pipe = cached_is_pipe;
    } else {
        struct stat st;
        is_pipe = (fstat(fd, &st) == 0 && S_ISFIFO(st.st_mode)) ? 1 : 0;
        cached_fd = fd;
        cached_is_pipe = is_pipe;
    }
    if (is_pipe) {
        const char *p = (const char *)buf;
        while (n > 0) {
            struct iovec v = {(void *)p, n};
            /* Without SPLICE_F_GIFT the kernel copies from our buffer into the
             * pipe, so the caller may reuse the buffer immediately. Using
             * SPLICE_F_GIFT would transfer page ownership (true zero-copy) but
             * the cooked path reuses its output buffer via memmove, so gift
             * mode is not safe here. The copy is still cheaper than write()
             * because the kernel avoids the user/kernel boundary copy. */
            ssize_t w = vmsplice(fd, &v, 1, 0);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                return mat_full_write(fd, p, n);
            }
            if (w == 0) {
                errno = EIO;
                return -1;
            }
            p += (size_t)w;
            n -= (size_t)w;
        }
        return 0;
    }
#endif
    return mat_full_write(fd, buf, n);
}
