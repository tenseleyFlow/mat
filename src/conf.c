#include "conf.h"
#include "cli.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ----- token array helpers ----- */

static void arr_push(char ***a, int *n, int *cap, char *s)
{
    if (s == NULL)
        return;
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        char **na = realloc(*a, (size_t)*cap * sizeof **a);
        if (na == NULL) {
            free(s);
            return;
        }
        *a = na;
    }
    (*a)[(*n)++] = s;
}

void mat_tokens_free(char **arr, int n)
{
    for (int i = 0; i < n; i++)
        free(arr[i]);
    free(arr);
}

int mat_tokenize(const char *s, bool comments, char ***out)
{
    char **arr = NULL;
    int n = 0, cap = 0;

    size_t tcap = 0, tlen = 0;
    char *t = NULL;
    bool in_tok = false;
    char quote = 0;

    for (const char *p = s; *p; p++) {
        char c = *p;
        if (quote) {
            if (c == quote)
                quote = 0;
            else {
                if (tlen + 1 >= tcap) {
                    size_t nc = tcap ? tcap * 2 : 32;
                    char *nb = realloc(t, nc);
                    if (nb == NULL)
                        break;
                    t = nb;
                    tcap = nc;
                }
                t[tlen++] = c;
            }
            in_tok = true;
            continue;
        }
        if (comments && c == '#' && !in_tok) {
            while (p[1] && p[1] != '\n')
                p++;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            in_tok = true;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (in_tok) {
                if (t == NULL) {
                    t = malloc(1);
                    if (!t)
                        break;
                    tcap = 1;
                }
                t[tlen] = '\0';
                arr_push(&arr, &n, &cap, strdup(t));
                tlen = 0;
                in_tok = false;
            }
            continue;
        }
        if (tlen + 1 >= tcap) {
            size_t nc = tcap ? tcap * 2 : 32;
            char *nb = realloc(t, nc);
            if (nb == NULL)
                break;
            t = nb;
            tcap = nc;
        }
        t[tlen++] = c;
        in_tok = true;
    }
    if (in_tok) {
        if (t == NULL) {
            t = malloc(1);
            if (!t) {
                *out = arr;
                return n;
            }
            tcap = 1;
        }
        t[tlen] = '\0';
        arr_push(&arr, &n, &cap, strdup(t));
    }
    free(t);
    *out = arr;
    return n;
}

/* ----- config sources ----- */

static char *read_whole_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        st.st_size > (1 << 20)) {
        close(fd);
        return NULL;
    }
    size_t sz = (size_t)st.st_size;
    char *buf = malloc(sz + 1);
    if (buf == NULL) {
        close(fd);
        return NULL;
    }
    size_t got = 0;
    while (got < sz) {
        ssize_t r = read(fd, buf + got, sz - got);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (r == 0)
            break;
        got += (size_t)r;
    }
    close(fd);
    buf[got] = '\0';
    return buf;
}

static void append_tokens(char ***a, int *n, int *cap, const char *str,
                          bool comments)
{
    char **t;
    int tn = mat_tokenize(str, comments, &t);
    for (int i = 0; i < tn; i++)
        arr_push(a, n, cap, t[i]); /* transfer ownership of each token */
    free(t);
}

static void append_file(char ***a, int *n, int *cap, const char *path)
{
    char *content = read_whole_file(path);
    if (content) {
        append_tokens(a, n, cap, content, true);
        free(content);
    }
}

static void append_env(char ***a, int *n, int *cap, const char *e1,
                       const char *e2, const char *prefix)
{
    const char *v = getenv(e1);
    if ((v == NULL || *v == '\0') && e2)
        v = getenv(e2);
    if (v == NULL || *v == '\0')
        return;
    size_t pl = strlen(prefix), vl = strlen(v);
    char *tok = malloc(pl + vl + 1);
    if (tok == NULL)
        return;
    memcpy(tok, prefix, pl);
    memcpy(tok + pl, v, vl + 1);
    arr_push(a, n, cap, tok);
}

static const char *system_config_path(void)
{
    static char buf[1024];
    const char *prefix = getenv("MAT_SYSTEM_CONFIG_PREFIX");
    if (prefix && *prefix) {
        snprintf(buf, sizeof buf, "%s/mat/config", prefix);
        return buf;
    }
    return "/etc/mat/config";
}

const char *mat_conf_user_path(char *buf, size_t n)
{
    const char *p = getenv("MAT_CONFIG_PATH");
    if (p && *p) {
        snprintf(buf, n, "%s", p);
        return buf;
    }
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(buf, n, "%s/mat/config", xdg);
        return buf;
    }
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(buf, n, "%s/.config/mat/config", home);
        return buf;
    }
    return NULL;
}

void mat_conf_print_template(void)
{
    fputs("# mat config — one option per line; '#' starts a comment.\n"
          "# Lines here are applied as defaults; the command line overrides.\n"
          "# Save to the path shown by `mat --config-file`.\n"
          "\n"
          "# Show the full frame by default on a terminal:\n"
          "#--style=full\n"
          "\n"
          "# Default decoration components:\n"
          "#--style=numbers,grid,header\n"
          "\n"
          "# Tab width in the frame:\n"
          "#--tabs=4\n"
          "\n"
          "# Long-line handling: auto|never|character|word\n"
          "#--wrap=auto\n"
          "\n"
          "# Color theme (see --list-themes):\n"
          "#--theme=dark\n"
          "\n"
          "# Paging: auto|never|always\n"
          "#--paging=auto\n"
          "\n"
          "# Color: auto|never|always\n"
          "#--color=auto\n",
          stdout);
}

void mat_conf_apply(struct config *cfg, bool no_config)
{
    char **toks = NULL;
    int n = 0, cap = 0;

    if (!no_config) {
        append_file(&toks, &n, &cap, system_config_path());
        char ubuf[1024];
        if (mat_conf_user_path(ubuf, sizeof ubuf))
            append_file(&toks, &n, &cap, ubuf);
    }

    const char *opts = getenv("MAT_OPTS");
    if (opts == NULL || *opts == '\0')
        opts = getenv("BAT_OPTS");
    if (opts && *opts)
        append_tokens(&toks, &n, &cap, opts, false);

    append_env(&toks, &n, &cap, "MAT_STYLE", "BAT_STYLE", "--style=");
    append_env(&toks, &n, &cap, "MAT_TABS", "BAT_TABS", "--tabs=");
    append_env(&toks, &n, &cap, "MAT_WRAP", NULL, "--wrap=");

    if (n == 0) {
        free(toks);
        return;
    }

    /* Parse the collected defaults through the normal CLI parser. */
    char **av = malloc((size_t)(n + 1) * sizeof *av);
    const char **scratch = malloc((size_t)(n + 1) * sizeof *scratch);
    if (av && scratch) {
        av[0] = (char *)"mat";
        for (int i = 0; i < n; i++)
            av[i + 1] = toks[i];
        bool sh = cfg->show_help, sv = cfg->show_version;
        mat_cli_parse(n + 1, av, cfg, scratch);
        /* Config sets defaults only — never input files or early-exit actions.
         */
        cfg->files = NULL;
        cfg->nfiles = 0;
        cfg->show_help = sh;
        cfg->show_version = sv;
    }
    free(av);
    free(scratch);
    mat_tokens_free(toks, n);
}
