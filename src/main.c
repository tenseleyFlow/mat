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
#include "compat.h"
#include "conf.h"
#include "config.h"
#include "cooked.h"
#include "err.h"
#include "fastpath.h"
#include "highlight.h"
#include "input.h"
#include "interactive.h"
#include "parallel.h"
#include "matpager.h"
#include "rangeprint.h"
#include "scan.h"
#include "syntax.h"

#include <signal.h>
#include <stdio.h>
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
    signal(SIGPIPE, SIG_IGN);

    if (argc > 0 && argv[0] && argv[0][0])
        mat_progname = basename_of(argv[0]);

    struct config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.tab_width = -1;    /* -1 = use the decorated-mode default (4) */
    cfg.squeeze_limit = 1; /* -s keeps one blank line unless overridden */

    /* Apply config-file + env defaults before the command line (which wins).
     * --no-config must be honored before the files are read, so detect it in
     * the raw argv first. */
    bool no_config = getenv("MAT_NO_CONFIG") != NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0)
            break;
        if (strcmp(argv[i], "--no-config") == 0)
            no_config = true;
    }
    mat_conf_apply(&cfg, no_config);

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
    if (cfg.show_config_file || cfg.show_config_dir) {
        char buf[1024];
        const char *path = mat_conf_user_path(buf, sizeof buf);
        if (path == NULL) {
            mat_warnx("could not determine config path");
        } else if (cfg.show_config_dir) {
            char *slash = strrchr(buf, '/');
            if (slash)
                *slash = '\0';
            printf("%s\n", buf);
        } else {
            printf("%s\n", path);
        }
        free(files_out);
        return mat_status();
    }
    if (cfg.gen_config) {
        mat_conf_print_template();
        free(files_out);
        return 0;
    }

    if (cfg.diagnostic) {
        printf("mat %s\n", MAT_VERSION);
        printf("build: %s %s\n", __DATE__, __TIME__);
#ifdef HAVE_COPY_FILE_RANGE
        printf("copy_file_range: yes\n");
#else
        printf("copy_file_range: no\n");
#endif
#ifdef HAVE_SPLICE
        printf("splice: yes\n");
#else
        printf("splice: no\n");
#endif
#ifdef HAVE_VMSPLICE
        printf("vmsplice: yes\n");
#else
        printf("vmsplice: no\n");
#endif
#ifdef HAVE_POSIX_FADVISE
        printf("posix_fadvise: yes\n");
#else
        printf("posix_fadvise: no\n");
#endif
        printf("languages: %d\nthemes: %d\n", mat_hl_language_count(),
               mat_theme_count());
        free(files_out);
        return 0;
    }

    if (cfg.list_themes) {
        mat_theme_list();
        free(files_out);
        return 0;
    }
    if (cfg.list_languages) {
        mat_hl_list_languages();
        free(files_out);
        return 0;
    }
    if (cfg.theme && mat_theme_set(cfg.theme) != 0) {
        fprintf(stderr, "%s: unknown theme '%s' (--list-themes for options)\n",
                mat_progname, cfg.theme);
        free(files_out);
        return 1;
    }

    /* --detect-syntax: print the resolved syntax for each input and exit. The
     * resolver is Sprint 07's deliverable; the highlighter (Sprint 08) consumes
     * the same names. */
    if (cfg.detect_syntax) {
        static const char *const stdin_only[] = {"-"};
        const char *const *files = cfg.nfiles ? cfg.files : stdin_only;
        size_t nf = cfg.nfiles ? cfg.nfiles : 1;
        for (size_t i = 0; i < nf; i++) {
            bool is_stdin = false;
            unsigned char first[256];
            size_t fn = 0;
            int fd = mat_open_input(files[i], &is_stdin);
            if (fd >= 0) {
                ssize_t r = read(fd, first, sizeof first);
                if (r > 0)
                    fn = (size_t)r;
                mat_close_input(fd, is_stdin, files[i]);
            }
            const char *detect_name =
                is_stdin ? (cfg.file_name ? cfg.file_name : "") : files[i];
            const char *disp =
                is_stdin ? (cfg.file_name ? cfg.file_name : "STDIN") : files[i];
            printf("%s: %s\n", disp,
                   mat_syntax_detect(&cfg, detect_name, first, fn));
        }
        free(files_out);
        return mat_status();
    }

    cfg.stdout_is_tty = isatty(STDOUT_FILENO) == 1;

    /* Decorations are opt-in: render the frame only when explicitly requested
     * (--decorations=always / --pretty, or --style on a TTY). Otherwise mat is
     * plain cat: cooked path for transforms, zero-copy fast path for raw. */
    bool deco_on;
    if (cfg.decorations == MAT_WHEN_ALWAYS)
        deco_on = true;
    else if (cfg.decorations == MAT_WHEN_NEVER)
        deco_on = false;
    else
        deco_on = cfg.style_given && cfg.stdout_is_tty;

    if (deco_on)
        cfg.style = cfg.style_given ? cfg.style : MAT_STYLE_DEFAULT;

    /* Line ranges (-r) select which lines print, so they take a line-aware path
     * regardless of decorations: decorated output gets the frame, plain output
     * the raw bytes. Does not page. */
    if (cfg.ranges.n > 0) {
        mat_scan_init();
        mat_rangeprint_run(&cfg, deco_on);
        free(files_out);
        return mat_status();
    }

    /* Paging (bespoke, via lib/paige): only on a terminal, for a single input,
     * and only in decorated mode or when forced — plain `mat file` still dumps
     * like cat. mat_page returns 1 to ask us to stream instead (fits one
     * screen, or no usable tty). */
    bool want_page = cfg.paging != MAT_WHEN_NEVER && cfg.stdout_is_tty &&
                     cfg.nfiles <= 1 &&
                     (deco_on || cfg.paging == MAT_WHEN_ALWAYS);
    if (want_page) {
        mat_scan_init();
        if (mat_page(&cfg, deco_on) == 0) {
            free(files_out);
            return mat_status();
        }
    }

    if (deco_on) {
        mat_scan_init();
        if (!mat_parallel_run(&cfg))
            mat_interactive_run(&cfg);
    } else if (cfg.xform != 0) {
        mat_scan_init(); /* select SIMD byte scanners for this CPU */
        mat_cooked_run(&cfg);
    } else {
        mat_fastpath_run(&cfg);
    }

    free(files_out);
    return mat_status();
}
