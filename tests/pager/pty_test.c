/*
 * pty_test — drive mat's bespoke pager inside a pseudo-terminal.
 *
 * Pages a 200-line file with `mat --pretty` on a 10-row terminal and checks the
 * decoration gutter and navigation. The mat binary path is argv[1].
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

static void read_screen(int fd, char *buf, size_t cap, int idle_ms)
{
    size_t len = 0;
    for (;;) {
        struct pollfd p = {fd, POLLIN, 0};
        if (poll(&p, 1, idle_ms) <= 0)
            break;
        ssize_t n = read(fd, buf + len, cap - len - 1);
        if (n <= 0)
            break;
        len += (size_t)n;
        if (len >= cap - 1)
            break;
    }
    buf[len] = '\0';
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
    alarm(15);

    char tmpl[] = "/tmp/mat_pager_XXXXXX";
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
        perror("forkpty");
        unlink(tmpl);
        return 1;
    }
    if (pid == 0) {
        execl(argv[1], argv[1], "--pretty", "--color=never", tmpl,
              (char *)NULL);
        _exit(127);
    }

    char buf[1 << 16];
    int fails = 0;

    read_screen(master, buf, sizeof buf, 400);
    if (!has(buf, "   1") || !has(buf, "row 001")) {
        printf("FAIL: pager did not show the numbered gutter / first line\n");
        fails++;
    }
    if (has(buf, "row 050")) {
        printf("FAIL: pager showed more than a screenful\n");
        fails++;
    }

    (void)!write(master, "G", 1); /* bottom */
    read_screen(master, buf, sizeof buf, 400);
    if (!has(buf, "row 200")) {
        printf("FAIL: 'G' did not reach the last line\n");
        fails++;
    }

    (void)!write(master, "g", 1); /* top */
    read_screen(master, buf, sizeof buf, 400);
    if (!has(buf, "row 001")) {
        printf("FAIL: 'g' did not return to the top\n");
        fails++;
    }

    /* j scrolls down one line */
    (void)!write(master, "j", 1);
    (void)!write(master, "j", 1);
    (void)!write(master, "j", 1);
    read_screen(master, buf, sizeof buf, 400);
    if (!has(buf, "row 004")) {
        printf("FAIL: 'jjj' did not scroll to row 004\n");
        fails++;
    }

    /* k scrolls back up */
    (void)!write(master, "k", 1);
    read_screen(master, buf, sizeof buf, 400);
    if (!has(buf, "row 003")) {
        printf("FAIL: 'k' did not scroll back to row 003\n");
        fails++;
    }

    /* space pages down */
    (void)!write(master, "g", 1); /* back to top first */
    read_screen(master, buf, sizeof buf, 400);
    (void)!write(master, " ", 1);
    read_screen(master, buf, sizeof buf, 400);
    if (has(buf, "row 001")) {
        printf("FAIL: space did not page down\n");
        fails++;
    }

    (void)!write(master, "q", 1);
    read_screen(master, buf, sizeof buf, 300);
    int status;
    if (waitpid(pid, &status, WNOHANG) == 0) {
        read_screen(master, buf, sizeof buf, 300);
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
