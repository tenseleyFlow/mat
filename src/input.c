#include "input.h"
#include "err.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int mat_open_input(const char *name, bool *is_stdin)
{
    if (name == NULL || strcmp(name, "-") == 0) {
        *is_stdin = true;
        return STDIN_FILENO;
    }
    *is_stdin = false;

    int fd = open(name, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        mat_warn(name);
        return -1;
    }
    return fd;
}

void mat_close_input(int fd, bool is_stdin, const char *name)
{
    if (is_stdin)
        return; /* never close the shared stdin */
    if (close(fd) < 0)
        mat_warn(name ? name : "close");
}

bool mat_input_is_output(int in_fd, const struct stat *in_st, const char *name)
{
    if (S_ISFIFO(in_st->st_mode) || S_ISSOCK(in_st->st_mode))
        return false;

    struct stat ost;
    if (fstat(STDOUT_FILENO, &ost) != 0)
        return false;
    if (in_st->st_dev != ost.st_dev || in_st->st_ino != ost.st_ino)
        return false;

    off_t in_pos = lseek(in_fd, 0, SEEK_CUR);
    if (in_pos < 0)
        return false;
    int oflags = fcntl(STDOUT_FILENO, F_GETFL);
    int whence = (oflags >= 0 && (oflags & O_APPEND)) ? SEEK_END : SEEK_CUR;
    off_t out_pos = lseek(STDOUT_FILENO, 0, whence);
    if (in_pos < out_pos) {
        fprintf(stderr, "%s: %s: input file is output file\n", mat_progname,
                name);
        mat_fail();
        return true;
    }
    return false;
}
