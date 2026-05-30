/*
 * iobuf.h — buffer-sizing policy and robust writing.
 *
 * Hides two secrets: (1) how big a copy buffer should be for a given fd, and
 * (2) how to write a full buffer in the face of partial writes and EINTR.
 */
#ifndef MAT_IOBUF_H
#define MAT_IOBUF_H

#include <stddef.h>
#include <sys/types.h>

/*
 * Choose an I/O buffer size for moving bytes from in_fd to out_fd.
 * Regular files get a memory-aware size (capped); pipes/devices follow
 * st_blksize floored at the page size. Returns a byte count > 0.
 */
size_t mat_iobuf_size(int in_fd, int out_fd);

/*
 * Write exactly n bytes, retrying short writes and EINTR. EAGAIN is treated as
 * an error (we never put stdout in non-blocking mode).
 * Returns 0 on success, -1 on error (errno set).
 */
int mat_full_write(int fd, const void *buf, size_t n);

/*
 * Like mat_full_write but uses vmsplice when available (Linux) and fd is a
 * pipe, avoiding the final userspace→kernel copy. Falls back to write()
 * on non-Linux or non-pipe fds.
 */
int mat_pipe_write(int fd, const void *buf, size_t n);

#endif /* MAT_IOBUF_H */
