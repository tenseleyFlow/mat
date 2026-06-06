/*
 * hl_test — regression test for stateful syntax highlighting under the pager.
 *
 * Markdown ``` code fences are a multi-line (stateful) construct: whether a
 * line is "inside a fence" depends on the ``` lines above it. The pager renders
 * only the visible window, at arbitrary scroll offsets, and may render a line
 * more than once per frame and out of order. If the host threads one persistent
 * lexer state across those calls, the fence parity desyncs and code/prose
 * colors swap on scroll. This test scrolls line-by-line through a fence taller
 * than the screen and asserts the colors never invert.
 *
 * The mat binary path is argv[1]. Skips (exit 0) when no pty is available
 * unless MAT_TEST_STRICT is set (CI), matching the other pager pty tests.
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__)
#include <util.h>
#else
#include <libutil.h>
#endif

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* MT_STRING in mat's default theme renders SGR 32 (green); that is the color of
 * text *inside* a code fence. Outside a fence, code-fence interior text would
 * be lexed as prose and get a different color. The test only needs:
 * fence-interior lines are green, prose lines are not. */
enum { GREEN = 32 };

static void on_alarm(int sig)
{
    (void)sig;
    const char *m = "FAIL: hl_test timed out\n";
    (void)!write(2, m, strlen(m));
    _exit(2);
}

/* Drain the pty until it is idle for ~idle_ms (a settled frame) or the budget
 * runs out. Returns the bytes captured. */
static size_t drain(int fd, char *buf, size_t cap, int idle_ms, int budget_ms)
{
    size_t len = 0;
    int waited = 0, since = 0;
    while (waited < budget_ms && since < idle_ms && len < cap - 1) {
        struct pollfd pfd = {fd, POLLIN, 0};
        int pr = poll(&pfd, 1, 10);
        waited += 10;
        if (pr <= 0) {
            since += 10;
            continue;
        }
        ssize_t n = read(fd, buf + len, cap - len - 1);
        if (n <= 0)
            break;
        len += (size_t)n;
        since = 0;
    }
    buf[len] = '\0';
    return len;
}

/* Single linear pass over a rendered frame: track the active SGR content color,
 * and at every occurrence of `needle` assert it is (want_green) or is not
 * GREEN. Returns 0 on the first violation. One pass, p always advances. */
static int check(const char *frame, const char *needle, int want_green,
                 const char *what)
{
    size_t nlen = strlen(needle);
    int color = -1; /* active content color (last SGR param) */
    for (const char *p = frame; *p;) {
        if (p[0] == '\x1b' && p[1] == '[') {
            const char *q = p + 2;
            int val = 0, have = 0;
            while (*q && *q != 'm') {
                if (*q >= '0' && *q <= '9') {
                    val = val * 10 + (*q - '0');
                    have = 1;
                } else if (*q == ';') {
                    val = 0;
                    have = 0;
                }
                q++;
            }
            if (*q == 'm') {
                color = have ? val : 0;
                p = q + 1;
            } else {
                p = q; /* unterminated: jump to the '\0' */
            }
            continue;
        }
        if (strncmp(p, needle, nlen) == 0) {
            int is_green = (color == GREEN);
            if (want_green && !is_green) {
                printf(
                    "FAIL: %s '%s' not green (color=%d) — fence state lost\n",
                    what, needle, color);
                return 0;
            }
            if (!want_green && is_green) {
                printf("FAIL: %s '%s' is green (color=%d) — fence inverted\n",
                       what, needle, color);
                return 0;
            }
            p += nlen;
            continue;
        }
        p++;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s MAT_BINARY\n", argv[0]);
        return 2;
    }
    signal(SIGALRM, on_alarm);
    alarm(60);

    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir)
        dir = "/tmp";
    char path[4096];
    /* .md suffix so mat detects markdown (the bug is markdown-fence specific).
     */
    snprintf(path, sizeof path, "%s/mat_hl_XXXXXX.md", dir);
    int fd = mkstemps(path, 3);
    if (fd < 0) {
        perror("mkstemps");
        return 1;
    }
    /* Prose, then a fence taller than the 8-row screen, then prose. Distinct
     * markers so the test can find code-interior vs prose lines. */
    FILE *f = fdopen(fd, "w");
    fputs("# Doc\n", f);
    for (int i = 0; i < 4; i++)
        fprintf(f, "PROSELINE top %d\n", i);
    fputs("```c\n", f);
    for (int i = 1; i <= 16; i++)
        fprintf(f, "CODELINE %02d here\n", i);
    fputs("```\n", f);
    for (int i = 0; i < 6; i++)
        fprintf(f, "PROSELINE bot %d\n", i);
    fclose(f);

    struct winsize ws;
    memset(&ws, 0, sizeof ws);
    ws.ws_row = 8;
    ws.ws_col = 80;

    int master;
    pid_t pid = forkpty(&master, NULL, NULL, &ws);
    if (pid < 0) {
        if (getenv("MAT_TEST_STRICT")) {
            fprintf(stderr, "FAIL: forkpty: %s (MAT_TEST_STRICT)\n",
                    strerror(errno));
            unlink(path);
            return 1;
        }
        printf("skip - pty unavailable (%s)\n", strerror(errno));
        unlink(path);
        return 0;
    }
    if (pid == 0) {
        execl(argv[1], argv[1], "--paging=always", "--decorations=always",
              "--color=always", "--style=plain", path, (char *)NULL);
        _exit(127);
    }

    char buf[1 << 16];
    int fails = 0;
    drain(master, buf, sizeof buf, 250, 4000); /* first paint */

    /* Scroll line-by-line down through the fence. On every settled frame, fence
     * interior must stay green and prose must stay non-green. Pre-fix, the
     * parity flips on alternating presses. */
    for (int step = 0; step < 14 && !fails; step++) {
        (void)!write(master, "j", 1);
        size_t n = drain(master, buf, sizeof buf, 250, 4000);
        if (n == 0)
            continue;
        if (!check(buf, "CODELINE", 1, "code-fence line") ||
            !check(buf, "PROSELINE", 0, "prose line")) {
            printf("       (violation at scroll step %d)\n", step + 1);
            fails++;
        }
    }

    /* Drain while waiting so mat is not blocked writing into a full pty buffer
     * (which would keep it from processing 'q'); SIGKILL as a hard backstop. */
    (void)!write(master, "q", 1);
    int status = 0;
    for (int i = 0; i < 50; i++) {
        if (waitpid(pid, &status, WNOHANG) != 0)
            break;
        struct pollfd pfd = {master, POLLIN, 0};
        char tmp[4096];
        if (poll(&pfd, 1, 10) > 0 && read(master, tmp, sizeof tmp) <= 0)
            break;
    }
    if (waitpid(pid, &status, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    close(master);
    unlink(path);

    if (fails) {
        printf("FAIL: syntax highlighting swapped on scroll\n");
        return 1;
    }
    printf("pty: pager syntax-highlight fence stability OK\n");
    return 0;
}
