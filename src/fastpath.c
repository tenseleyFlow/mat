#include "fastpath.h"
#include "compat.h"
#include "err.h"
#include "input.h"
#include "iobuf.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

/* Copy one already-open fd to stdout using the provided scratch buffer.
 * Returns 0 normally, -1 if a stdout write failed (fatal for the whole run). */
static int copy_fd(int fd, const char *name, char *buf, size_t bufsz)
{
    for (;;) {
        ssize_t n = read(fd, buf, bufsz);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            mat_warn(name); /* read error: recoverable, stop this file */
            return 0;
        }
        if (n == 0)
            return 0; /* EOF */
        if (MAT_UNLIKELY(mat_full_write(STDOUT_FILENO, buf, (size_t)n) < 0)) {
            mat_warn("stdout"); /* write error: fatal */
            return -1;
        }
    }
}

void mat_fastpath_run(const struct config *cfg)
{
    /* Normalize "no operands" to a single stdin pass. */
    static const char *const stdin_only[] = {"-"};
    const char *const *files = cfg->nfiles ? cfg->files : stdin_only;
    size_t nfiles = cfg->nfiles ? cfg->nfiles : 1;

    char *buf = NULL;
    size_t bufsz = 0;

    for (size_t i = 0; i < nfiles; i++) {
        bool is_stdin = false;
        int fd = mat_open_input(files[i], &is_stdin);
        if (fd < 0)
            continue; /* mat_open_input already warned + recorded failure */

        /* Allocate once, sized from the first input; reuse across files. */
        if (buf == NULL) {
            bufsz = mat_iobuf_size(fd, STDOUT_FILENO);
            buf = (char *)malloc(bufsz);
            if (buf == NULL) {
                mat_warnx("out of memory");
                mat_close_input(fd, is_stdin, files[i]);
                return;
            }
        }

        int rc = copy_fd(fd, is_stdin ? "stdin" : files[i], buf, bufsz);
        mat_close_input(fd, is_stdin, files[i]);
        if (rc < 0)
            break; /* fatal stdout error */
    }

    free(buf);
}
