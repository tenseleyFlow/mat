/*
 * config.h — the runtime configuration POD.
 *
 * Pure data, no logic. Populated by cli.c, consumed everywhere. The transform
 * flag bitset is the gate for the cooked path; in Sprint 00 no transforms are
 * implemented yet, so a zero bitset means "take the fast path".
 */
#ifndef MAT_CONFIG_H
#define MAT_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "range.h"

/* Transform flags — any set bit forces the cooked path (Sprint 02). */
enum mat_xform {
    MAT_X_NUMBER = 1u << 0,        /* -n */
    MAT_X_NUMBER_NB = 1u << 1,     /* -b */
    MAT_X_SQUEEZE = 1u << 2,       /* -s */
    MAT_X_SHOW_ENDS = 1u << 3,     /* -e / -E */
    MAT_X_SHOW_TABS = 1u << 4,     /* -t / -T */
    MAT_X_SHOW_NONPRINT = 1u << 5, /* -v */
};

/* When to colorize / decorate (bat-style tri-states). */
enum mat_when { MAT_WHEN_AUTO, MAT_WHEN_NEVER, MAT_WHEN_ALWAYS };

/* How decorated output treats a binary file. Default skips with a notice. */
enum mat_binary { MAT_BINARY_NO_PRINTING, MAT_BINARY_AS_TEXT };

/* Long-line handling in the decoration frame. AUTO == CHARACTER (bat default).
 */
enum mat_wrap {
    MAT_WRAP_AUTO,
    MAT_WRAP_NEVER,
    MAT_WRAP_CHARACTER,
    MAT_WRAP_WORD,
};

/* --style components (the decoration frame). */
enum mat_style {
    MAT_S_NUMBERS = 1u << 0,     /* line-number gutter */
    MAT_S_GRID = 1u << 1,        /* │ / ─ / ┬┼┴ box drawing */
    MAT_S_HEADER = 1u << 2,      /* "File: <name>" */
    MAT_S_HEADER_SIZE = 1u << 3, /* "Size: <n> B" */
    MAT_S_RULE = 1u << 4,        /* horizontal rule between files */
    MAT_S_SNIP = 1u << 5,        /* separator between disjoint ranges */
    MAT_S_CHANGES = 1u << 6,     /* git change markers in the gutter */
};
#define MAT_STYLE_FULL                                                         \
    (MAT_S_NUMBERS | MAT_S_GRID | MAT_S_HEADER | MAT_S_HEADER_SIZE |           \
     MAT_S_RULE | MAT_S_SNIP)
#define MAT_STYLE_DEFAULT                                                      \
    (MAT_S_NUMBERS | MAT_S_GRID | MAT_S_HEADER | MAT_S_SNIP)

struct config {
    /* Inputs: pointers into argv, not owned. "-" means stdin. */
    const char *const *files;
    size_t nfiles;

    unsigned xform;     /* OR of enum mat_xform; 0 => fast path eligible */
    bool unbuffered;    /* -u */
    bool stdout_is_tty; /* isatty(STDOUT_FILENO), cached once */

    /* Decorations (Sprint 03). mat is plain cat unless decorations are
     * explicitly requested. */
    enum mat_when color;
    enum mat_when decorations;
    unsigned style;       /* OR of enum mat_style */
    bool style_given;     /* --style was explicitly set */
    int term_width;       /* explicit columns, or <=0 to auto-detect */
    enum mat_wrap wrap;   /* long-line wrapping in the frame */
    int tab_width;        /* -1 = default (4 in the frame), 0 = no expansion */
    enum mat_when paging; /* page the output through the bespoke pager */

    /* Line selection / emphasis (Sprint 06). */
    struct mat_rangeset ranges;     /* -r: lines to print (empty = all) */
    struct mat_rangeset highlights; /* -H: lines to emphasize */
    int squeeze_limit;              /* -s: max blank run to keep (default 1) */

    /* Encoding / mapping (Sprint 07). */
    enum mat_binary binary; /* --binary: how decorated output treats binary */
    enum mat_when
        strip_ansi; /* --strip-ansi: strip input escapes (auto=deco) */

    /* Syntax mapping inputs (Sprint 07; consumed by the highlighter, Sprint
     * 08). All pointers point into argv and are not owned. */
    const char *language;           /* -l/--language: explicit syntax */
    const char *map_glob[32];       /* --map-syntax glob part */
    const char *map_syntax[32];     /* --map-syntax syntax part */
    int nmaps;                      /* number of --map-syntax entries */
    const char *ignored_suffix[16]; /* --ignored-suffix to strip before ext */
    int nsuffix;                    /* number of --ignored-suffix entries */
    const char *file_name;          /* --file-name: name/detection for stdin */
    const char *fallback_syntax;    /* --fallback-syntax when detection fails */
    bool detect_syntax;             /* --detect-syntax: print syntax and exit */
    const char *theme;              /* --theme: named color theme */
    bool list_themes;               /* --list-themes */
    bool list_languages;            /* -L/--list-languages */

    /* Git change markers (Sprint 09). */
    bool diff;       /* -d/--diff: show git change markers in the gutter */
    bool diagnostic; /* --diagnostic: print build info and exit */

    /* Config. */
    bool no_config; /* --no-config: skip config files */

    /* Early-exit actions. */
    bool show_help;
    bool show_version;
    bool show_config_file; /* --config-file */
    bool show_config_dir;  /* --config-dir */
    bool gen_config;       /* --generate-config-file */
};

#endif /* MAT_CONFIG_H */
