/*
 * pty_test — drive mat's bespoke pager inside a pseudo-terminal.
 *
 * Pages a 200-line file with `mat --pretty` on a 10-row terminal and checks the
 * decoration gutter and navigation. The mat binary path is argv[1].
 *
 * Two robustness properties matter here because this runs in `make test`, which
 * packagers run at install time (e.g. the AUR check()):
 *   1. If the environment has no pty at all (minimal chroot, container without
 *      /dev/pts), this is not a mat bug — we SKIP rather than fail. Set
 *      MAT_TEST_STRICT=1 (CI, where a pty must exist) to make that fatal
 * instead.
 *   2. Reads wait for the *expected output* to appear (bounded by a budget and
 * a global alarm), not for a fixed idle window. A slow or loaded box just takes
 * longer; it doesn't race a timeout and flake.
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__)
#include <util.h>
#else
#include <libutil.h> /* BSD: needs struct winsize from above */
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static void on_alarm(int sig)
{
    (void)sig;
    const char *m = "FAIL: mat pty_test timed out\n";
    (void)!write(2, m, strlen(m));
    _exit(2);
}

/*
 * Read from the pty into buf until `want` appears (or, when `want` is NULL,
 * until the screen produces output and then goes quiet for a moment). Bounded
 * by `budget_ms` so a real regression — the expected text never showing —
 * returns and lets the caller's assertion fail with a clear message, rather
 * than hanging to the global alarm. On a slow box the text just arrives later,
 * within budget.
 */
static void read_until(int fd, char *buf, size_t cap, const char *want,
                       int budget_ms)
{
    const int slice = 100; /* poll granularity, ms */
    /* Silence (ms) that means the screen has settled, used when want==NULL. */
    const int settle = 300;
    size_t len = 0;
    int waited = 0, idle = 0, got_any = 0;

    buf[0] = '\0';
    for (;;) {
        if (want && strstr(buf, want))
            return; /* found it — done early */

        struct pollfd p = {fd, POLLIN, 0};
        int r = poll(&p, 1, slice);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (r == 0) {
            waited += slice;
            if (got_any && !want && (idle += slice) >= settle)
                return; /* produced output, then went quiet: settled */
            if (waited >= budget_ms)
                return; /* budget spent; caller asserts on what we have */
            continue;
        }
        ssize_t n = read(fd, buf + len, cap - len - 1);
        if (n <= 0)
            return;
        len += (size_t)n;
        buf[len] = '\0';
        got_any = 1;
        idle = 0;
        if (len >= cap - 1)
            return;
    }
}

static int has(const char *h, const char *n)
{
    return strstr(h, n) != NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: pty_test <mat-binary>\n");
        return 2;
    }
    signal(SIGALRM, on_alarm);
    /* Hard backstop against a hung pager; healthy runs take only seconds. */
    alarm(45);

    /* Honor $TMPDIR so the test runs in build sandboxes that point it elsewhere
     * or restrict /tmp (the shell test runners already do this). */
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir)
        tmpdir = "/tmp";
    char tmpl[4096];
    snprintf(tmpl, sizeof tmpl, "%s/mat_pager_XXXXXX", tmpdir);
    int fd = mkstemp(tmpl);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }
    for (int i = 1; i <= 200; i++) {
        char line[64];
        int m =
            snprintf(line, sizeof line, "row %03d the quick brown fox\n", i);
        (void)!write(fd, line, (size_t)m);
    }
    close(fd);

    struct winsize ws;
    memset(&ws, 0, sizeof ws);
    ws.ws_row = 10;
    ws.ws_col = 60;

    int master;
    pid_t pid = forkpty(&master, NULL, NULL, &ws);
    if (pid < 0) {
        /* No pty available (e.g. a chroot/container without /dev/pts). That's
         * an environment limitation, not a mat failure — skip, unless strict.
         */
        int e = errno;
        unlink(tmpl);
        if (getenv("MAT_TEST_STRICT")) {
            fprintf(stderr, "FAIL: forkpty: %s (MAT_TEST_STRICT set)\n",
                    strerror(e));
            return 1;
        }
        printf("skip - mat pager pty (no pty available: %s)\n", strerror(e));
        return 0;
    }
    if (pid == 0) {
        execl(argv[1], argv[1], "--pretty", "--color=never", tmpl,
              (char *)NULL);
        _exit(127);
    }

    char buf[1 << 16];
    int fails = 0;
    /* Per-read wait ceiling, ms — generous for slow or loaded boxes. */
    const int budget = 4000;

    read_until(master, buf, sizeof buf, "row 001", budget);
    if (!has(buf, "   1") || !has(buf, "row 001")) {
        printf("FAIL: pager did not show the numbered gutter / first line\n");
        fails++;
    }
    if (has(buf, "row 050")) {
        printf("FAIL: pager showed more than a screenful\n");
        fails++;
    }

    (void)!write(master, "G", 1); /* bottom */
    read_until(master, buf, sizeof buf, "row 200", budget);
    if (!has(buf, "row 200")) {
        printf("FAIL: 'G' did not reach the last line\n");
        fails++;
    }

    (void)!write(master, "g", 1); /* top */
    read_until(master, buf, sizeof buf, "row 001", budget);
    if (!has(buf, "row 001")) {
        printf("FAIL: 'g' did not return to the top\n");
        fails++;
    }

    /* j scrolls down one line */
    (void)!write(master, "j", 1);
    (void)!write(master, "j", 1);
    (void)!write(master, "j", 1);
    read_until(master, buf, sizeof buf, "row 004", budget);
    if (!has(buf, "row 004")) {
        printf("FAIL: 'jjj' did not scroll to row 004\n");
        fails++;
    }

    /* k scrolls back up */
    (void)!write(master, "k", 1);
    read_until(master, buf, sizeof buf, "row 003", budget);
    if (!has(buf, "row 003")) {
        printf("FAIL: 'k' did not scroll back to row 003\n");
        fails++;
    }

    /* space pages down */
    (void)!write(master, "g", 1); /* back to top first */
    read_until(master, buf, sizeof buf, "row 001", budget);
    (void)!write(master, " ", 1);
    /* settle on the new screen, then assert the old top line is gone */
    read_until(master, buf, sizeof buf, NULL, budget);
    if (has(buf, "row 001")) {
        printf("FAIL: space did not page down\n");
        fails++;
    }

    (void)!write(master, "q", 1);
    read_until(master, buf, sizeof buf, NULL, 1000);
    int status;
    if (waitpid(pid, &status, WNOHANG) == 0) {
        read_until(master, buf, sizeof buf, NULL, 1000);
        if (waitpid(pid, &status, WNOHANG) == 0) {
            kill(pid, SIGTERM);
            waitpid(pid, &status, 0);
        }
    }
    close(master);
    unlink(tmpl);

    if (fails == 0) {
        printf("mat pager: gutter + nav (G/g/j/k/space/q) OK\n");
        return 0;
    }
    return 1;
}
