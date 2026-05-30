#include "err.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *mat_progname = "mat";

static int had_error;

void mat_fail(void)
{
    had_error = 1;
}

void mat_warn(const char *ctx)
{
    int e = errno;
    fprintf(stderr, "%s: %s: %s\n", mat_progname, ctx, strerror(e));
    had_error = 1;
}

void mat_warnx(const char *msg)
{
    fprintf(stderr, "%s: %s\n", mat_progname, msg);
    had_error = 1;
}

int mat_status(void)
{
    return had_error ? 1 : 0;
}
