#include "term.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

int mat_term_width(int explicit_width)
{
    if (explicit_width > 0)
        return explicit_width;

    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;

    const char *cols = getenv("COLUMNS");
    if (cols && *cols) {
        char *end;
        long v = strtol(cols, &end, 10);
        if (*end == '\0' && v > 0 && v < 100000)
            return (int)v;
    }
    return 80;
}

bool mat_stdout_is_tty(void)
{
    return isatty(STDOUT_FILENO) == 1;
}

bool mat_no_color(void)
{
    const char *nc = getenv("NO_COLOR");
    return nc != NULL && *nc != '\0';
}
