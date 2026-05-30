/*
 * main.c — entry point and the central TTY/pipe split.
 *
 * The split is the spine of the whole program (audit 02): decide once, up top,
 * which pipeline runs. In Sprint 00 only the fast path exists; the cooked path
 * (transform flags) and the interactive/decorated path (TTY niceties) are
 * stubs that land in Sprints 02 and 03. Keeping the branch here from day one
 * means later sprints slot in without restructuring main.
 */
#include "cli.h"
#include "config.h"
#include "cooked.h"
#include "err.h"
#include "fastpath.h"
#include "scan.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *basename_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int main(int argc, char **argv)
{
    if (argc > 0 && argv[0] && argv[0][0])
        mat_progname = basename_of(argv[0]);

    struct config cfg;
    memset(&cfg, 0, sizeof cfg);

    const char **files_out =
        malloc(sizeof(*files_out) * (size_t)(argc > 0 ? argc : 1));
    if (files_out == NULL) {
        mat_warnx("out of memory");
        return 1;
    }

    if (mat_cli_parse(argc, argv, &cfg, files_out) != 0) {
        free(files_out);
        return 1;
    }
    if (cfg.show_help) {
        mat_print_usage();
        free(files_out);
        return 0;
    }
    if (cfg.show_version) {
        mat_print_version();
        free(files_out);
        return 0;
    }

    cfg.stdout_is_tty = isatty(STDOUT_FILENO) == 1;

    /* The split: transforms take the cooked path, otherwise the zero-copy
     * fast path. (TTY decorations arrive in Sprint 03.) */
    if (cfg.xform != 0) {
        mat_scan_init(); /* select SIMD byte scanners for this CPU */
        mat_cooked_run(&cfg);
    } else {
        mat_fastpath_run(&cfg);
    }

    free(files_out);
    return mat_status();
}
