#include "cooked.h"
#include "counter.h"
#include "err.h"
#include "expand.h"
#include "input.h"
#include "iobuf.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * This is a close port of coreutils' cat() cooked loop. The structure — a
 * sentinel newline at the end of the input buffer, the newlines state machine,
 * pending_cr for cross-buffer CRLF, and the threshold flush — is preserved so
 * the output is byte-identical to GNU cat. The two differences are mat's
 * pre-formatted counter (counter.c) and the 256-entry expansion table
 * (expand.c) in place of cat's inline branch chain.
 */

#define COOKED_INSIZE ((size_t)(128 * 1024))
#define COOKED_OUTSIZE ((size_t)(128 * 1024))
/* Headroom: between flush checks one line of input may expand 4x, plus a line
 * number. Matches coreutils' allocation. */
#define COOKED_OUTCAP                                                          \
    (COOKED_OUTSIZE - 1 + COOKED_INSIZE * 4 + MAT_LINE_BUF_LEN)

struct cooked {
    bool number;          /* -n or -b: number lines */
    bool number_nonblank; /* -b: don't number blank lines */
    bool show_ends;       /* -E (and -e): '$' at line end */
    bool show_tabs;       /* -T (and -t): tabs as ^I */
    bool show_nonprint;   /* -v (and -e, -t): quote nonprintables */
    bool squeeze;         /* -s: collapse repeated blank lines */
    struct mat_counter counter;
    struct mat_xtable xt; /* built only when show_nonprint */
    int newlines;         /* consecutive-newline state, persists across files */
    bool pending_cr;
    char *inbuf;
    char *outbuf;
    char *bpout;
};

/* Write [outbuf, bpout) and reset. Returns 0 ok, -1 on a fatal write error. */
static int flush_pending(struct cooked *c)
{
    size_t n = (size_t)(c->bpout - c->outbuf);
    if (n > 0) {
        if (mat_full_write(STDOUT_FILENO, c->outbuf, n) < 0) {
            mat_warn("stdout");
            return -1;
        }
        c->bpout = c->outbuf;
    }
    return 0;
}

/* Cook one input fd to stdout. Returns 0 (ok or recoverable read error) or -1
 * (fatal stdout write error). Run state is saved back into *c before return. */
static int cook_fd(struct cooked *c, int fd, const char *name)
{
    int newlines = c->newlines;
    char *inbuf = c->inbuf;
    char *outbuf = c->outbuf;
    char *bpout = c->bpout;
    char *eob = inbuf;
    char *bpin = eob + 1; /* force an initial read */
    unsigned char ch;

    for (;;) {
        do {
            /* Flush once at least OUTSIZE bytes are buffered. */
            if (outbuf + COOKED_OUTSIZE <= bpout) {
                char *wp = outbuf;
                size_t rem;
                do {
                    if (mat_full_write(STDOUT_FILENO, wp, COOKED_OUTSIZE) < 0) {
                        mat_warn("stdout");
                        c->bpout = outbuf;
                        c->newlines = newlines;
                        return -1;
                    }
                    wp += COOKED_OUTSIZE;
                    rem = (size_t)(bpout - wp);
                } while (COOKED_OUTSIZE <= rem);
                memmove(outbuf, wp, rem);
                bpout = outbuf + rem;
            }

            if (bpin > eob) {
                ssize_t n;
                do {
                    n = read(fd, inbuf, COOKED_INSIZE);
                } while (n < 0 && errno == EINTR);
                if (n < 0) {
                    mat_warn(name);
                    c->bpout = bpout;
                    c->newlines = newlines;
                    return 0;
                }
                if (n == 0) {
                    c->bpout = bpout;
                    c->newlines = newlines;
                    return 0;
                }
                bpin = inbuf;
                eob = inbuf + n;
                *eob = '\n'; /* sentinel */
            } else {
                /* A real newline. */
                if (++newlines > 0) {
                    if (newlines >= 2) {
                        newlines = 2; /* cap so the counter can't wrap */
                        if (c->squeeze) {
                            ch = (unsigned char)*bpin++;
                            continue;
                        }
                    }
                    if (c->number && !c->number_nonblank)
                        bpout = stpcpy(bpout, mat_counter_next(&c->counter));
                }
                if (c->show_ends) {
                    if (c->pending_cr) {
                        *bpout++ = '^';
                        *bpout++ = 'M';
                        c->pending_cr = false;
                    }
                    *bpout++ = '$';
                }
                *bpout++ = '\n';
            }
            ch = (unsigned char)*bpin++;
        } while (ch == '\n');

        /* ch is not a newline here. */
        if (c->pending_cr) {
            *bpout++ = '\r';
            c->pending_cr = false;
        }
        if (newlines >= 0 && c->number)
            bpout = stpcpy(bpout, mat_counter_next(&c->counter));

        if (c->show_nonprint) {
            const struct mat_xtable *xt = &c->xt;
            for (;;) {
                if (ch == '\n') {
                    newlines = -1;
                    break;
                }
                unsigned len = xt->len[ch];
                memcpy(bpout, xt->buf[ch], len);
                bpout += len;
                ch = (unsigned char)*bpin++;
            }
        } else {
            for (;;) {
                if (ch == '\t' && c->show_tabs) {
                    *bpout++ = '^';
                    *bpout++ = (char)(ch + 64);
                } else if (ch != '\n') {
                    if (ch == '\r' && *bpin == '\n' && c->show_ends) {
                        if (bpin == eob) {
                            c->pending_cr = true;
                        } else {
                            *bpout++ = '^';
                            *bpout++ = 'M';
                        }
                    } else {
                        *bpout++ = (char)ch;
                    }
                } else {
                    newlines = -1;
                    break;
                }
                ch = (unsigned char)*bpin++;
            }
        }
    }
}

void mat_cooked_run(const struct config *cfg)
{
    struct cooked c;
    memset(&c, 0, sizeof c);
    c.number = (cfg->xform & (MAT_X_NUMBER | MAT_X_NUMBER_NB)) != 0;
    c.number_nonblank = (cfg->xform & MAT_X_NUMBER_NB) != 0;
    c.show_ends = (cfg->xform & MAT_X_SHOW_ENDS) != 0;
    c.show_tabs = (cfg->xform & MAT_X_SHOW_TABS) != 0;
    c.show_nonprint = (cfg->xform & MAT_X_SHOW_NONPRINT) != 0;
    c.squeeze = (cfg->xform & MAT_X_SQUEEZE) != 0;

    c.inbuf = malloc(COOKED_INSIZE + 1);
    c.outbuf = malloc(COOKED_OUTCAP);
    if (c.inbuf == NULL || c.outbuf == NULL) {
        mat_warnx("out of memory");
        free(c.inbuf);
        free(c.outbuf);
        return;
    }
    mat_counter_init(&c.counter);
    if (c.show_nonprint)
        mat_xtable_build(&c.xt, c.show_tabs);
    c.newlines = 0;
    c.pending_cr = false;
    c.bpout = c.outbuf;

    static const char *const stdin_only[] = {"-"};
    const char *const *files = cfg->nfiles ? cfg->files : stdin_only;
    size_t nfiles = cfg->nfiles ? cfg->nfiles : 1;

    for (size_t i = 0; i < nfiles; i++) {
        bool is_stdin = false;
        int fd = mat_open_input(files[i], &is_stdin);
        if (fd < 0)
            continue;

        const char *label = is_stdin ? "stdin" : files[i];
        struct stat in_st;
        if (fstat(fd, &in_st) == 0 && mat_input_is_output(fd, &in_st, label)) {
            mat_close_input(fd, is_stdin, files[i]);
            continue;
        }

        int rc = cook_fd(&c, fd, label);
        mat_close_input(fd, is_stdin, files[i]);
        if (rc < 0) {
            free(c.inbuf);
            free(c.outbuf);
            return; /* fatal write error: stop, nothing more to flush */
        }
    }

    flush_pending(&c);
    free(c.inbuf);
    free(c.outbuf);
}
