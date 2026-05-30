#include "input.h"
#include "err.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int mat_open_input(const char *name, bool *is_stdin)
{
    if (name == NULL || strcmp(name, "-") == 0) {
        *is_stdin = true;
        return STDIN_FILENO;
    }
    *is_stdin = false;

    int fd = open(name, O_RDONLY);
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
