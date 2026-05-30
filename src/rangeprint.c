#include "rangeprint.h"
#include "err.h"
#include "input.h"
#include "iobuf.h"
#include "linesrc.h"
#include "range.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RP_BUFCAP ((size_t)(128 * 1024))

struct out {
    char *buf;
    size_t pos;
    bool failed;
};

static void out_flush(struct out *o)
{
    if (o->pos && !o->failed) {
        if (mat_full_write(STDOUT_FILENO, o->buf, o->pos) < 0) {
            mat_warn("stdout");
            o->failed = true;
        }
    }
    o->pos = 0;
}

static void out_put(struct out *o, const unsigned char *d, size_t n)
{
    if (o->failed)
        return;
    if (n >= RP_BUFCAP) {
        out_flush(o);
        if (!o->failed && mat_full_write(STDOUT_FILENO, d, n) < 0) {
            mat_warn("stdout");
            o->failed = true;
        }
        return;
    }
    if (o->pos + n > RP_BUFCAP)
        out_flush(o);
    memcpy(o->buf + o->pos, d, n);
    o->pos += n;
}

static void print_file(const struct config *cfg, const char *file,
                       struct out *o)
{
    bool is_stdin = false;
    int fd = mat_open_input(file, &is_stdin);
    if (fd < 0)
        return;
    struct mat_linesrc src;
    if (!mat_linesrc_open(&src, fd)) {
        mat_warn(is_stdin ? "stdin" : file);
        mat_close_input(fd, is_stdin, file);
        return;
    }

    /* A bounded selection stops at its highest line; an open-ended or last-N
     * selection must read to EOF (and last-N needs the total to resolve). */
    long maxl = mat_rangeset_max_line(&cfg->ranges);
    long total = 0;
    if (cfg->ranges.needs_total)
        total = (long)mat_linesrc_total(&src);

    for (long L = 1; L <= maxl && !o->failed; L++) {
        const unsigned char *d;
        size_t len;
        if (!mat_linesrc_line(&src, (size_t)(L - 1), &d, &len))
            break;
        if (!mat_rangeset_contains(&cfg->ranges, L, total))
            continue;
        out_put(o, d, len);
        out_put(o, (const unsigned char *)"\n", 1);
    }

    mat_linesrc_free(&src);
    mat_close_input(fd, is_stdin, file);
}

void mat_rangeprint_run(const struct config *cfg)
{
    struct out o = {malloc(RP_BUFCAP), 0, false};
    if (o.buf == NULL) {
        mat_warnx("out of memory");
        return;
    }
    static const char *const stdin_only[] = {"-"};
    const char *const *files = cfg->nfiles ? cfg->files : stdin_only;
    size_t nfiles = cfg->nfiles ? cfg->nfiles : 1;
    for (size_t i = 0; i < nfiles && !o.failed; i++)
        print_file(cfg, files[i], &o);
    out_flush(&o);
    free(o.buf);
}
