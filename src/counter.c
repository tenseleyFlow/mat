#include "counter.h"

#include <string.h>

void mat_counter_init(struct mat_counter *c)
{
    memset(c->buf, ' ', MAT_LINE_BUF_LEN);
    c->buf[MAT_LINE_BUF_LEN - 3] = '0';
    c->buf[MAT_LINE_BUF_LEN - 2] = '\t';
    c->buf[MAT_LINE_BUF_LEN - 1] = '\0';
    c->print = c->buf + MAT_LINE_BUF_LEN - 8; /* 6-wide field + tab */
    c->start = c->buf + MAT_LINE_BUF_LEN - 3;
    c->end = c->buf + MAT_LINE_BUF_LEN - 3;
}

const char *mat_counter_next(struct mat_counter *c)
{
    char *endp = c->end;
    for (;;) {
        if ((*endp)++ < '9')
            return c->print;
        *endp-- = '0';
        if (endp < c->start)
            break;
    }
    if (c->start > c->buf)
        *--c->start = '1';
    else
        *c->buf = '>';
    if (c->start < c->print)
        c->print--;
    return c->print;
}
