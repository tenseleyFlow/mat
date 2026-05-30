/*
 * input.h — opening inputs.
 *
 * Hides the file-vs-stdin distinction and the "-" convention. The SAME_INODE
 * input==output guard (mat f > f) lands in Sprint 01; this is the Sprint 00
 * minimum.
 */
#ifndef MAT_INPUT_H
#define MAT_INPUT_H

#include <stdbool.h>

/*
 * Open the named input. name == NULL or "-" yields STDIN_FILENO and sets
 * *is_stdin = true (the caller must NOT close it). On error returns -1 with a
 * diagnostic already emitted via mat_warn.
 */
int mat_open_input(const char *name, bool *is_stdin);

/* Close a non-stdin input, warning on failure. No-op for stdin. */
void mat_close_input(int fd, bool is_stdin, const char *name);

#endif /* MAT_INPUT_H */
