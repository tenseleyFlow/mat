/*
 * iobuf.h — buffer-sizing policy and robust writing.
 *
 * Hides two secrets: (1) how big a copy buffer should be for a given fd, and
 * (2) how to write a full buffer in the face of partial writes and EINTR.
 */
#ifndef MAT_IOBUF_H
#define MAT_IOBUF_H

#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * Choose an I/O buffer size from the input's already-known stat. Regular files
 * get a memory-aware size (capped); pipes/devices follow st_blksize floored at
 * the page size. The caller passes the stat it already has, so this stays a
 * pure function (no syscall). Returns a byte count > 0.
 */
size_t mat_iobuf_size(const struct stat *in_st, int out_fd);

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
