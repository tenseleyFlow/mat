#include "cli.h"
#include "err.h"

#include <stdio.h>
#include <string.h>

#ifndef MAT_VERSION
#define MAT_VERSION "0.0.0-dev"
#endif

void mat_print_usage(void)
{
    fputs(
        "usage: mat [OPTION]... [FILE]...\n"
        "Concatenate FILE(s) to standard output. With no FILE, or when FILE is\n"
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
        "  -u            (ignored; output is always unbuffered on the fast path)\n"
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
    case 'n': cfg->xform |= MAT_X_NUMBER; return true;
    case 'b': cfg->xform |= MAT_X_NUMBER_NB; return true;
    case 's': cfg->xform |= MAT_X_SQUEEZE; return true;
    case 'e': cfg->xform |= MAT_X_SHOW_NONPRINT | MAT_X_SHOW_ENDS; return true;
    case 't': cfg->xform |= MAT_X_SHOW_NONPRINT | MAT_X_SHOW_TABS; return true;
    case 'v': cfg->xform |= MAT_X_SHOW_NONPRINT; return true;
    case 'A': cfg->xform |= MAT_X_SHOW_NONPRINT | MAT_X_SHOW_ENDS |
                            MAT_X_SHOW_TABS; return true;
    case 'E': cfg->xform |= MAT_X_SHOW_ENDS; return true;
    case 'T': cfg->xform |= MAT_X_SHOW_TABS; return true;
    case 'u': cfg->unbuffered = true; return true;
    default:  return false;
    }
}

int mat_cli_parse(int argc, char **argv, struct config *cfg,
                  const char **files_out)
{
    size_t nf = 0;
    bool opts_done = false;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (!opts_done && a[0] == '-' && a[1] != '\0') {
            if (strcmp(a, "--") == 0) { opts_done = true; continue; }
            if (strcmp(a, "--help") == 0) { cfg->show_help = true; return 0; }
            if (strcmp(a, "--version") == 0) { cfg->show_version = true; return 0; }
            if (a[1] == '-') {
                fprintf(stderr, "%s: unrecognized option '%s'\n", mat_progname, a);
                return -1;
            }
            /* Short cluster, e.g. -vET. */
            for (const char *p = a + 1; *p; p++) {
                if (*p == 'h') { cfg->show_help = true; return 0; }
                if (*p == 'V') { cfg->show_version = true; return 0; }
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
