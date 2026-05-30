#include "fastpath.h"
#include "compat.h"
#include "err.h"
#include "input.h"
#include "iobuf.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * The zero-copy ladder. For a no-transform copy we try, in order:
 *   1. copy_file_range  — regular file -> regular file, in the kernel.
 *   2. splice           — file -> pipe, in the kernel (Linux).
 *   3. read()/write()   — the universal fallback (Sprint 00 loop).
 * A method is selected by the kinds of the input and output descriptors; a
 * method that fails "before moving any bytes" yields to the next one. Because
 * copy_file_range and splice advance the input fd offset, falling back after a
 * partial transfer simply resumes from where the kernel left off — no data is
 * duplicated or lost.
 */

/* Per-method outcome. */
#define R_DONE 1     /* this file is fully handled; go to the next file */
#define R_FALLBACK 0 /* method not applicable / unsupported; try next method   \
                      */
#define R_ABORT (-1) /* fatal stdout write error; stop the whole run */

/* copy_file_range moves up to ~1 GiB per call (GiB-aligned, << SSIZE_MAX). */
#define MAT_CFR_MAX ((size_t)1 << 30)

/* splice request length per call. */
#define MAT_SPLICE_LEN ((size_t)1 << 20)

/* Below this size, a regular file is faster via read()/write() than splice
 * (coreutils measured ~32 KiB as the crossover; it also sidesteps zero-length
 * edge cases). */
#define MAT_SPLICE_MIN_SIZE ((off_t)32768)

/* posix_fadvise(SEQUENTIAL) only pays off on large inputs; skip the syscall for
 * small ones. */
#define MAT_FADVISE_MIN_SIZE ((off_t)(4 << 20))

/* --- pure eligibility predicates (always compiled; unit-tested) --- */

bool mat_cfr_eligible(bool in_isreg, bool out_isreg, bool cfr_ok)
{
    return cfr_ok && out_isreg && in_isreg;
}

bool mat_splice_eligible(bool in_isreg, off_t in_size, bool out_ispipe,
                         bool splice_ok)
{
    bool small_reg = in_isreg && in_size <= MAT_SPLICE_MIN_SIZE;
    return splice_ok && out_ispipe && !small_reg;
}

bool mat_cfr_fallback_errno(int e)
{
    return e == ENOSYS || e == EINVAL || e == EBADF || e == EXDEV ||
           e == ETXTBSY || e == EPERM || e == EFBIG
#ifdef ENOTSUP
           || e == ENOTSUP
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || EOPNOTSUPP != ENOTSUP)
           || e == EOPNOTSUPP
#endif
        ;
}

struct copier {
    int out_fd;
    int out_flags; /* lazily fetched F_GETFL; -2 = not fetched */
    bool out_has_stat;
    bool out_isreg;
    bool out_ispipe;
    bool out_pipe_grown;
    dev_t out_dev;
    ino_t out_ino;
    bool cfr_ok;    /* copy_file_range still worth trying this run */
    bool splice_ok; /* splice still worth trying this run */
    char *buf;      /* read/write fallback buffer, allocated on first use */
    size_t bufsz;
};

static void copier_init(struct copier *c)
{
    c->out_fd = STDOUT_FILENO;
    c->out_flags = -2;
    c->out_pipe_grown = false;
    c->buf = NULL;
    c->bufsz = 0;
#if HAVE_COPY_FILE_RANGE
    c->cfr_ok = true;
#else
    c->cfr_ok = false;
#endif
#if HAVE_SPLICE
    c->splice_ok = true;
#else
    c->splice_ok = false;
#endif

    struct stat st;
    if (fstat(c->out_fd, &st) == 0) {
        c->out_has_stat = true;
        c->out_isreg = S_ISREG(st.st_mode) != 0;
        c->out_ispipe = S_ISFIFO(st.st_mode) != 0;
        c->out_dev = st.st_dev;
        c->out_ino = st.st_ino;
    } else {
        c->out_has_stat = false;
        c->out_isreg = false;
        c->out_ispipe = false;
    }
}

/* Refuse "mat f > f": copying a file onto itself would loop until the disk
 * fills. Mirrors coreutils' dev/ino + position check. Returns true (and warns)
 * when the input is the output and we must skip it. */
static bool is_self_overwrite(struct copier *c, int in_fd,
                              const struct stat *in_st, const char *name)
{
    if (!c->out_has_stat)
        return false;
    if (S_ISFIFO(in_st->st_mode) || S_ISSOCK(in_st->st_mode))
        return false;
    if (in_st->st_dev != c->out_dev || in_st->st_ino != c->out_ino)
        return false;

    off_t in_pos = lseek(in_fd, 0, SEEK_CUR);
    if (in_pos < 0)
        return false;
    if (c->out_flags == -2)
        c->out_flags = fcntl(c->out_fd, F_GETFL);
    int whence =
        (c->out_flags >= 0 && (c->out_flags & O_APPEND)) ? SEEK_END : SEEK_CUR;
    off_t out_pos = lseek(c->out_fd, 0, whence);
    if (in_pos < out_pos) {
        fprintf(stderr, "%s: %s: input file is output file\n", mat_progname,
                name);
        mat_fail();
        return true;
    }
    return false;
}

#if HAVE_COPY_FILE_RANGE
static int do_cfr(struct copier *c, int in_fd, const char *name)
{
    bool some = false;
    for (;;) {
        ssize_t r =
            copy_file_range(in_fd, NULL, c->out_fd, NULL, MAT_CFR_MAX, 0);
        if (r == 0)
            return some ? R_DONE : R_FALLBACK; /* EOF, or empty/proc quirk */
        if (r < 0) {
            if (errno == ENOSYS)
                c->cfr_ok = false; /* kernel lacks it: never try again */
            if (mat_cfr_fallback_errno(errno))
                return R_FALLBACK; /* offset advanced; next method resumes */
            mat_warn(name);        /* genuine error: record, do not fall back */
            return R_DONE;
        }
        some = true;
    }
}
#endif

#if HAVE_SPLICE
static void grow_pipe(int fd)
{
#if defined(F_SETPIPE_SZ)
    /* Bigger pipe buffers mean fewer splice round-trips. Best effort. */
    (void)fcntl(fd, F_SETPIPE_SZ, (int)(1 << 20));
#else
    (void)fd;
#endif
}

/* Direct file -> pipe splice. Only used when stdout is a pipe. */
static int do_splice(struct copier *c, int in_fd, const char *name)
{
    if (!c->out_pipe_grown) {
        grow_pipe(c->out_fd);
        c->out_pipe_grown = true;
    }
    bool some = false;
    for (;;) {
        ssize_t n =
            splice(in_fd, NULL, c->out_fd, NULL, MAT_SPLICE_LEN, SPLICE_F_MORE);
        if (n == 0)
            return some ? R_DONE : R_FALLBACK;
        if (n < 0) {
            if (errno == ENOSYS)
                c->splice_ok = false;
            if (!some)
                return R_FALLBACK; /* e.g. EINVAL: these fds can't splice */
            mat_warn(name);        /* partial then error */
            return R_DONE;
        }
        some = true;
    }
}
#endif

/* read()/write() fallback with a reused, adaptively sized buffer. */
static int do_read_write(struct copier *c, int in_fd, const char *name)
{
    if (c->buf == NULL) {
        c->bufsz = mat_iobuf_size(in_fd, c->out_fd);
        c->buf = malloc(c->bufsz);
        if (c->buf == NULL) {
            mat_warnx("out of memory");
            return R_DONE;
        }
    }
    for (;;) {
        ssize_t n = read(in_fd, c->buf, c->bufsz);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            mat_warn(name);
            return R_DONE;
        }
        if (n == 0)
            return R_DONE;
        if (mat_full_write(c->out_fd, c->buf, (size_t)n) < 0) {
            mat_warn("stdout");
            return R_ABORT;
        }
    }
}

/* Copy one open input to stdout, choosing the best available method. */
static int copy_one(struct copier *c, int in_fd, const struct stat *in_st,
                    const char *name)
{
    bool in_isreg = S_ISREG(in_st->st_mode) != 0;

    if (in_isreg && in_st->st_size >= MAT_FADVISE_MIN_SIZE) {
#if HAVE_POSIX_FADVISE
        (void)posix_fadvise(in_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
    }

#if HAVE_COPY_FILE_RANGE
    if (mat_cfr_eligible(in_isreg, c->out_isreg, c->cfr_ok)) {
        int r = do_cfr(c, in_fd, name);
        if (r != R_FALLBACK)
            return r;
    }
#endif

#if HAVE_SPLICE
    if (mat_splice_eligible(in_isreg, in_st->st_size, c->out_ispipe,
                            c->splice_ok)) {
        int r = do_splice(c, in_fd, name);
        if (r != R_FALLBACK)
            return r;
    }
#endif

    return do_read_write(c, in_fd, name);
}

void mat_fastpath_run(const struct config *cfg)
{
    static const char *const stdin_only[] = {"-"};
    const char *const *files = cfg->nfiles ? cfg->files : stdin_only;
    size_t nfiles = cfg->nfiles ? cfg->nfiles : 1;

    struct copier c;
    copier_init(&c);

    for (size_t i = 0; i < nfiles; i++) {
        bool is_stdin = false;
        int fd = mat_open_input(files[i], &is_stdin);
        if (fd < 0)
            continue;

        const char *label = is_stdin ? "stdin" : files[i];
        struct stat in_st;
        if (fstat(fd, &in_st) < 0) {
            mat_warn(label);
            mat_close_input(fd, is_stdin, files[i]);
            continue;
        }

        if (is_self_overwrite(&c, fd, &in_st, label)) {
            mat_close_input(fd, is_stdin, files[i]);
            continue;
        }

        int rc = copy_one(&c, fd, &in_st, label);
        mat_close_input(fd, is_stdin, files[i]);
        if (rc == R_ABORT)
            break;
    }

    free(c.buf);
}
