#include "interactive.h"
#include "err.h"
#include "input.h"
#include "iobuf.h"
#include "scan.h"
#include "term.h"
#include "width.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Box-drawing glyphs as explicit UTF-8 bytes (portable, no \u execution-charset
 * assumptions). Each is one display column, three bytes. */
#define BX_H "\xe2\x94\x80" /* ─ */
#define BX_V "\xe2\x94\x82" /* │ */
#define BX_D "\xe2\x94\xac" /* ┬ */
#define BX_X "\xe2\x94\xbc" /* ┼ */
#define BX_U "\xe2\x94\xb4" /* ┴ */

#define COL_GUTTER "\x1b[38;5;238m" /* dim grey */
#define COL_RESET "\x1b[0m"

#define IP_BUFCAP ((size_t)(128 * 1024))

struct ip {
    bool numbers, grid, header, header_size, rule;
    bool color;
    int term_width;
    int panel_width;   /* line-number field + trailing space, or 0 */
    int content_width; /* columns available for content after the gutter */
    int tab_width;     /* 0 = no tab expansion */
    enum mat_wrap wrap;
    char *buf;
    size_t pos;
    char *pend; /* partial line carried across reads */
    size_t pend_cap, pend_len;
    char *wbuf; /* tab-expanded line, reused */
    size_t wbuf_cap, wbuf_len;
    bool failed; /* sticky fatal write error */
};

static void ip_flush(struct ip *p)
{
    if (p->pos && !p->failed) {
        if (mat_full_write(STDOUT_FILENO, p->buf, p->pos) < 0) {
            mat_warn("stdout");
            p->failed = true;
        }
    }
    p->pos = 0;
}

static void ip_write(struct ip *p, const char *d, size_t n)
{
    if (p->failed)
        return;
    if (n >= IP_BUFCAP) {
        ip_flush(p);
        if (!p->failed && mat_full_write(STDOUT_FILENO, d, n) < 0) {
            mat_warn("stdout");
            p->failed = true;
        }
        return;
    }
    if (p->pos + n > IP_BUFCAP)
        ip_flush(p);
    memcpy(p->buf + p->pos, d, n);
    p->pos += n;
}

static void ip_str(struct ip *p, const char *s)
{
    ip_write(p, s, strlen(s));
}

static void ip_repeat(struct ip *p, const char *s, size_t slen, int count)
{
    for (int i = 0; i < count; i++)
        ip_write(p, s, slen);
}

/* A horizontal rule with the given junction glyph at the panel column. */
static void hrule(struct ip *p, const char *junction)
{
    if (p->color)
        ip_str(p, COL_GUTTER);
    if (p->panel_width > 0) {
        ip_repeat(p, BX_H, 3, p->panel_width);
        ip_str(p, junction);
        int rest = p->term_width - p->panel_width - 1;
        if (rest > 0)
            ip_repeat(p, BX_H, 3, rest);
    } else {
        ip_repeat(p, BX_H, 3, p->term_width);
    }
    if (p->color)
        ip_str(p, COL_RESET);
    ip_str(p, "\n");
}

/* The gutter prefix for a content line. On continuation (wrapped) lines the
 * number is blanked but the column width is preserved so the grid stays
 * aligned. */
static void gutter(struct ip *p, unsigned long n, bool continuation)
{
    if (p->color)
        ip_str(p, COL_GUTTER);
    if (p->numbers) {
        if (continuation) {
            ip_repeat(p, " ", 1, p->panel_width);
        } else {
            char num[32];
            int len = snprintf(num, sizeof num, "%4lu ", n);
            if (len > 0)
                ip_write(p, num, (size_t)len);
        }
    }
    if (p->grid && p->panel_width > 0)
        ip_str(p, BX_V " ");
    if (p->color)
        ip_str(p, COL_RESET);
}

/* A header text line ("File: name" / "Size: ..."), framed if grid is on. */
static void header_line(struct ip *p, const char *label, const char *value)
{
    if (p->panel_width > 0) {
        if (p->color)
            ip_str(p, COL_GUTTER);
        ip_repeat(p, " ", 1, p->panel_width);
        if (p->grid)
            ip_str(p, BX_V " ");
        if (p->color)
            ip_str(p, COL_RESET);
    }
    ip_str(p, label);
    ip_str(p, value);
    ip_str(p, "\n");
}

static void fmt_size(unsigned long long b, char *out, size_t n)
{
    if (b < 1024ULL)
        snprintf(out, n, "%llu B", b);
    else if (b < 1024ULL * 1024)
        snprintf(out, n, "%.1f KiB", (double)b / 1024.0);
    else if (b < 1024ULL * 1024 * 1024)
        snprintf(out, n, "%.1f MiB", (double)b / (1024.0 * 1024));
    else
        snprintf(out, n, "%.1f GiB", (double)b / (1024.0 * 1024 * 1024));
}

static void print_header(struct ip *p, const char *name, int fd, bool is_stdin)
{
    if (p->grid)
        hrule(p, BX_D);
    header_line(p, "File: ", is_stdin ? "STDIN" : name);
    if (p->header_size && !is_stdin) {
        struct stat st;
        if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
            char sz[48];
            fmt_size((unsigned long long)st.st_size, sz, sizeof sz);
            header_line(p, "Size: ", sz);
        }
    }
    if (p->grid)
        hrule(p, BX_X);
}

static void pend_append(struct ip *p, const unsigned char *d, size_t n)
{
    if (p->pend_len + n > p->pend_cap) {
        size_t cap = p->pend_cap ? p->pend_cap * 2 : 8192;
        while (cap < p->pend_len + n)
            cap *= 2;
        char *nb = realloc(p->pend, cap);
        if (nb == NULL) {
            mat_warnx("out of memory");
            p->failed = true;
            return;
        }
        p->pend = nb;
        p->pend_cap = cap;
    }
    memcpy(p->pend + p->pend_len, d, n);
    p->pend_len += n;
}

static void wbuf_append(struct ip *p, const char *d, size_t n)
{
    if (p->wbuf_len + n > p->wbuf_cap) {
        size_t cap = p->wbuf_cap ? p->wbuf_cap * 2 : 8192;
        while (cap < p->wbuf_len + n)
            cap *= 2;
        char *nb = realloc(p->wbuf, cap);
        if (nb == NULL) {
            mat_warnx("out of memory");
            p->failed = true;
            return;
        }
        p->wbuf = nb;
        p->wbuf_cap = cap;
    }
    memcpy(p->wbuf + p->wbuf_len, d, n);
    p->wbuf_len += n;
}

/* Expand tabs in [d,len) into p->wbuf, advancing through display columns so
 * each tab lands on the next tab stop. */
static void expand_tabs(struct ip *p, const unsigned char *d, size_t len)
{
    p->wbuf_len = 0;
    if (p->tab_width <= 0) {
        wbuf_append(p, (const char *)d, len);
        return;
    }
    int col = 0;
    size_t i = 0;
    while (i < len && !p->failed) {
        if (d[i] == '\t') {
            int sp = p->tab_width - (col % p->tab_width);
            for (int k = 0; k < sp; k++)
                wbuf_append(p, " ", 1);
            col += sp;
            i++;
        } else if (d[i] < 0x80) {
            wbuf_append(p, (const char *)d + i, 1);
            col += 1;
            i++;
        } else {
            uint32_t cp;
            size_t cl = mat_utf8_decode(d + i, d + len, &cp);
            wbuf_append(p, (const char *)d + i, cl);
            col += mat_wcwidth(cp);
            i += cl;
        }
    }
}

static int run_width(const unsigned char *a, const unsigned char *b)
{
    int w = 0;
    while (a < b) {
        uint32_t cp;
        size_t cl = mat_utf8_decode(a, b, &cp);
        w += mat_wcwidth(cp);
        a += cl;
    }
    return w;
}

/* Emit one logical line: expand tabs, then wrap into width-sized segments with
 * a continuation gutter, matching bat's frame. */
static void emit_line(struct ip *p, unsigned long n, const unsigned char *d,
                      size_t len)
{
    expand_tabs(p, d, len);
    size_t wl = p->wbuf_len;

    if (p->wrap == MAT_WRAP_NEVER || p->content_width <= 0) {
        gutter(p, n, false);
        ip_write(p, p->wbuf, wl);
        ip_str(p, "\n");
        return;
    }

    const unsigned char *w = (const unsigned char *)p->wbuf;
    bool word = (p->wrap == MAT_WRAP_WORD);
    size_t seg = 0, i = 0, last_ws = 0;
    bool have_ws = false, first = true;
    int col = 0;
    while (i < wl) {
        uint32_t cp;
        size_t cl = mat_utf8_decode(w + i, w + wl, &cp);
        int cw = mat_wcwidth(cp);
        if (col + cw > p->content_width && i > seg) {
            size_t brk = i, next = i;
            if (word && have_ws && last_ws > seg) {
                brk = last_ws;      /* break at the last space */
                next = last_ws + 1; /* and drop it */
            }
            gutter(p, n, !first);
            ip_write(p, p->wbuf + seg, brk - seg);
            ip_str(p, "\n");
            first = false;
            seg = next;
            col = run_width(w + next, w + i);
            have_ws = false;
        }
        if (cp == ' ') {
            last_ws = i;
            have_ws = true;
        }
        col += cw;
        i += cl;
        if (p->failed)
            return;
    }
    gutter(p, n, !first);
    ip_write(p, p->wbuf + seg, wl - seg);
    ip_str(p, "\n");
}

static void print_body(struct ip *p, int fd, const char *name,
                       unsigned long *lineno)
{
    unsigned char rbuf[65536];
    p->pend_len = 0;
    for (;;) {
        ssize_t n = read(fd, rbuf, sizeof rbuf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            mat_warn(name);
            return;
        }
        if (n == 0)
            break;
        const unsigned char *s = rbuf;
        const unsigned char *end = rbuf + n;
        while (s < end) {
            const unsigned char *q = mat_scan_newline(s, end);
            if (q < end && p->pend_len == 0) {
                emit_line(p, (*lineno)++, s, (size_t)(q - s));
                s = q + 1;
            } else {
                pend_append(p, s, (size_t)(q - s));
                if (q < end) {
                    emit_line(p, (*lineno)++, (const unsigned char *)p->pend,
                              p->pend_len);
                    p->pend_len = 0;
                    s = q + 1;
                } else {
                    s = end;
                }
            }
            if (p->failed)
                return;
        }
    }
    if (p->pend_len > 0)
        emit_line(p, (*lineno)++, (const unsigned char *)p->pend, p->pend_len);
}

void mat_interactive_run(const struct config *cfg)
{
    struct ip p;
    memset(&p, 0, sizeof p);
    p.numbers = (cfg->style & MAT_S_NUMBERS) != 0;
    p.grid = (cfg->style & MAT_S_GRID) != 0;
    p.header = (cfg->style & MAT_S_HEADER) != 0;
    p.header_size = (cfg->style & MAT_S_HEADER_SIZE) != 0;
    p.rule = (cfg->style & MAT_S_RULE) != 0;
    p.term_width = mat_term_width(cfg->term_width);

    if (cfg->color == MAT_WHEN_ALWAYS)
        p.color = true;
    else if (cfg->color == MAT_WHEN_NEVER)
        p.color = false;
    else
        p.color = cfg->stdout_is_tty && !mat_no_color();

    p.panel_width = p.numbers ? 5 : 0; /* 4-wide number + 1 separator */
    /* Too-narrow terminal: drop the frame entirely. */
    if (p.panel_width > 0 && p.term_width < p.panel_width + 5) {
        p.numbers = false;
        p.grid = false;
        p.panel_width = 0;
    }

    p.wrap = cfg->wrap;
    p.tab_width = cfg->tab_width < 0 ? 4 : cfg->tab_width;
    /* Content columns after the gutter (numbers field + "│ " when gridded). */
    int gutter_vis = p.numbers ? (p.panel_width + (p.grid ? 2 : 0)) : 0;
    p.content_width = p.term_width - gutter_vis;
    if (p.content_width < 1)
        p.content_width = 1;

    p.buf = malloc(IP_BUFCAP);
    if (p.buf == NULL) {
        mat_warnx("out of memory");
        return;
    }

    static const char *const stdin_only[] = {"-"};
    const char *const *files = cfg->nfiles ? cfg->files : stdin_only;
    size_t nfiles = cfg->nfiles ? cfg->nfiles : 1;

    for (size_t i = 0; i < nfiles && !p.failed; i++) {
        bool is_stdin = false;
        int fd = mat_open_input(files[i], &is_stdin);
        if (fd < 0)
            continue;

        const char *label = is_stdin ? "stdin" : files[i];
        if (p.rule && i > 0)
            hrule(&p, BX_H); /* separator between files */
        if (p.header)
            print_header(&p, label, fd, is_stdin); /* ┬ … header … ┼ */
        else if (p.grid)
            hrule(&p, BX_D); /* grid without a header: just the top border */

        unsigned long lineno = 1;
        print_body(&p, fd, label, &lineno);

        if (p.grid)
            hrule(&p, BX_U); /* bottom border */
        mat_close_input(fd, is_stdin, files[i]);
    }

    ip_flush(&p);
    free(p.buf);
    free(p.pend);
    free(p.wbuf);
}
