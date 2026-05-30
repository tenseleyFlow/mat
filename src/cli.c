#include "cli.h"
#include "err.h"
#include "style.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAT_VERSION
#define MAT_VERSION "0.0.0-dev"
#endif

void mat_print_usage(void)
{
    fputs(
        "usage: mat [OPTION]... [FILE]...\n"
        "Concatenate FILE(s) to standard output. With no FILE, or when FILE "
        "is\n"
        "-, read standard input.\n\n"
        "  -n            number all output lines\n"
        "  -b            number nonempty output lines (overrides -n)\n"
        "  -s            squeeze repeated empty lines\n"
        "  -e            display $ at end of each line (implies -v)\n"
        "  -t            display TAB as ^I (implies -v)\n"
        "  -v            show nonprinting characters (^ and M- notation)\n"
        "  -A            equivalent to -vet\n"
        "  -E            display $ at end of each line\n"
        "  -T            display TAB as ^I\n"
        "  -u            (ignored; output is always unbuffered on the fast "
        "path)\n"
        "\n"
        "Decorations (opt-in; piped output stays plain like cat):\n"
        "  -p, --pretty         show the full frame (header, grid, numbers)\n"
        "  -S, --chop-long-lines  do not wrap (same as --wrap=never)\n"
        "      --style=LIST     numbers,grid,header,header-filesize,rule,"
        "snip\n"
        "                       presets plain,default,full (e.g. full,-grid)\n"
        "      --decorations=WHEN  auto|never|always\n"
        "      --color=WHEN        auto|never|always\n"
        "      --wrap=MODE         auto|never|character|word\n"
        "      --tabs=N            expand tabs to N columns (0 = off)\n"
        "      --terminal-width=N  columns for the frame\n"
        "\n"
        "Config (defaults from /etc/mat/config, ~/.config/mat/config, "
        "$MAT_OPTS,\n"
        "$MAT_STYLE/$MAT_TABS/$MAT_WRAP; the command line overrides):\n"
        "      --no-config              ignore config files\n"
        "      --config-file            print the config file path\n"
        "      --config-dir             print the config directory\n"
        "      --generate-config-file   print a config template\n"
        "\n"
        "      --help    display this help and exit\n"
        "      --version output version information and exit\n",
        stdout);
}

void mat_print_version(void)
{
    printf("mat %s\n", MAT_VERSION);
}

/* Map a classic cat short flag to transform bits. Returns false if unknown. */
static bool apply_short(char c, struct config *cfg)
{
    switch (c) {
    case 'n':
        cfg->xform |= MAT_X_NUMBER;
        return true;
    case 'b':
        cfg->xform |= MAT_X_NUMBER_NB;
        return true;
    case 's':
        cfg->xform |= MAT_X_SQUEEZE;
        return true;
    case 'e':
        cfg->xform |= MAT_X_SHOW_NONPRINT | MAT_X_SHOW_ENDS;
        return true;
    case 't':
        cfg->xform |= MAT_X_SHOW_NONPRINT | MAT_X_SHOW_TABS;
        return true;
    case 'v':
        cfg->xform |= MAT_X_SHOW_NONPRINT;
        return true;
    case 'A':
        cfg->xform |= MAT_X_SHOW_NONPRINT | MAT_X_SHOW_ENDS | MAT_X_SHOW_TABS;
        return true;
    case 'E':
        cfg->xform |= MAT_X_SHOW_ENDS;
        return true;
    case 'T':
        cfg->xform |= MAT_X_SHOW_TABS;
        return true;
    case 'u':
        cfg->unbuffered = true;
        return true;
    default:
        return false;
    }
}

/* Match a valued long option, accepting both --name=value and --name value.
 * Returns 1 on match (value in *val), 0 on no match, -1 on a missing argument.
 */
static int match_val(const char *arg, const char *name, int *i, int argc,
                     char **argv, const char **val)
{
    size_t nl = strlen(name);
    if (strncmp(arg, name, nl) != 0)
        return 0;
    if (arg[nl] == '=') {
        *val = arg + nl + 1;
        return 1;
    }
    if (arg[nl] == '\0') {
        if (*i + 1 >= argc) {
            fprintf(stderr, "%s: option '%s' requires an argument\n",
                    mat_progname, name);
            return -1;
        }
        *val = argv[++(*i)];
        return 1;
    }
    return 0; /* e.g. "--styleX" must not match "--style" */
}

static int parse_when(const char *v, enum mat_when *out)
{
    if (strcmp(v, "auto") == 0)
        *out = MAT_WHEN_AUTO;
    else if (strcmp(v, "never") == 0)
        *out = MAT_WHEN_NEVER;
    else if (strcmp(v, "always") == 0)
        *out = MAT_WHEN_ALWAYS;
    else
        return -1;
    return 0;
}

/* --pretty / -p: force the full decoration frame on. */
static void set_pretty(struct config *cfg)
{
    cfg->decorations = MAT_WHEN_ALWAYS;
    cfg->style = MAT_STYLE_FULL;
    cfg->style_given = true;
}

int mat_cli_parse(int argc, char **argv, struct config *cfg,
                  const char **files_out)
{
    size_t nf = 0;
    bool opts_done = false;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (!opts_done && a[0] == '-' && a[1] != '\0') {
            if (strcmp(a, "--") == 0) {
                opts_done = true;
                continue;
            }
            if (strcmp(a, "--help") == 0) {
                cfg->show_help = true;
                return 0;
            }
            if (strcmp(a, "--version") == 0) {
                cfg->show_version = true;
                return 0;
            }
            if (strcmp(a, "--pretty") == 0) {
                set_pretty(cfg);
                continue;
            }
            if (strcmp(a, "--no-config") == 0) {
                cfg->no_config = true;
                continue;
            }
            if (strcmp(a, "--config-file") == 0) {
                cfg->show_config_file = true;
                return 0;
            }
            if (strcmp(a, "--config-dir") == 0) {
                cfg->show_config_dir = true;
                return 0;
            }
            if (strcmp(a, "--generate-config-file") == 0) {
                cfg->gen_config = true;
                return 0;
            }
            if (strcmp(a, "--chop-long-lines") == 0) {
                cfg->wrap = MAT_WRAP_NEVER;
                continue;
            }
            if (a[1] == '-') {
                const char *val;
                int r;
                if ((r = match_val(a, "--style", &i, argc, argv, &val))) {
                    char err[64];
                    if (r < 0)
                        return -1;
                    if (mat_style_parse(val, &cfg->style, err, sizeof err)) {
                        fprintf(stderr, "%s: unknown --style component '%s'\n",
                                mat_progname, err);
                        return -1;
                    }
                    cfg->style_given = true;
                    continue;
                }
                if ((r = match_val(a, "--color", &i, argc, argv, &val))) {
                    if (r < 0 || parse_when(val, &cfg->color)) {
                        if (r >= 0)
                            fprintf(stderr,
                                    "%s: --color expects auto|never|"
                                    "always\n",
                                    mat_progname);
                        return -1;
                    }
                    continue;
                }
                if ((r = match_val(a, "--decorations", &i, argc, argv, &val))) {
                    if (r < 0 || parse_when(val, &cfg->decorations)) {
                        if (r >= 0)
                            fprintf(stderr,
                                    "%s: --decorations expects auto|"
                                    "never|always\n",
                                    mat_progname);
                        return -1;
                    }
                    continue;
                }
                if ((r = match_val(a, "--terminal-width", &i, argc, argv,
                                   &val))) {
                    if (r < 0)
                        return -1;
                    char *end;
                    long w = strtol(val, &end, 10);
                    if (*end != '\0' || w <= 0 || w >= 100000) {
                        fprintf(stderr, "%s: invalid --terminal-width '%s'\n",
                                mat_progname, val);
                        return -1;
                    }
                    cfg->term_width = (int)w;
                    continue;
                }
                if ((r = match_val(a, "--wrap", &i, argc, argv, &val))) {
                    if (r < 0)
                        return -1;
                    if (strcmp(val, "auto") == 0)
                        cfg->wrap = MAT_WRAP_AUTO;
                    else if (strcmp(val, "never") == 0)
                        cfg->wrap = MAT_WRAP_NEVER;
                    else if (strcmp(val, "character") == 0)
                        cfg->wrap = MAT_WRAP_CHARACTER;
                    else if (strcmp(val, "word") == 0)
                        cfg->wrap = MAT_WRAP_WORD;
                    else {
                        fprintf(stderr,
                                "%s: --wrap expects auto|never|"
                                "character|word\n",
                                mat_progname);
                        return -1;
                    }
                    continue;
                }
                if ((r = match_val(a, "--tabs", &i, argc, argv, &val))) {
                    if (r < 0)
                        return -1;
                    char *end;
                    long t = strtol(val, &end, 10);
                    if (*end != '\0' || t < 0 || t > 64) {
                        fprintf(stderr, "%s: invalid --tabs '%s'\n",
                                mat_progname, val);
                        return -1;
                    }
                    cfg->tab_width = (int)t;
                    continue;
                }
                fprintf(stderr, "%s: unrecognized option '%s'\n", mat_progname,
                        a);
                return -1;
            }
            /* Short cluster, e.g. -vET. */
            for (const char *p = a + 1; *p; p++) {
                if (*p == 'h') {
                    cfg->show_help = true;
                    return 0;
                }
                if (*p == 'V') {
                    cfg->show_version = true;
                    return 0;
                }
                if (*p == 'p') {
                    set_pretty(cfg);
                    continue;
                }
                if (*p == 'S') {
                    cfg->wrap = MAT_WRAP_NEVER; /* --chop-long-lines */
                    continue;
                }
                if (!apply_short(*p, cfg)) {
                    fprintf(stderr, "%s: invalid option -- '%c'\n",
                            mat_progname, *p);
                    return -1;
                }
            }
            continue;
        }

        /* Operand (including a bare "-"). */
        files_out[nf++] = a;
    }

    cfg->files = files_out;
    cfg->nfiles = nf;
    return 0;
}
