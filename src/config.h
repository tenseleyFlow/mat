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

/* Transform flags — any set bit forces the cooked path (Sprint 02). */
enum mat_xform {
    MAT_X_NUMBER = 1u << 0,        /* -n */
    MAT_X_NUMBER_NB = 1u << 1,     /* -b */
    MAT_X_SQUEEZE = 1u << 2,       /* -s */
    MAT_X_SHOW_ENDS = 1u << 3,     /* -e / -E */
    MAT_X_SHOW_TABS = 1u << 4,     /* -t / -T */
    MAT_X_SHOW_NONPRINT = 1u << 5, /* -v */
};

struct config {
    /* Inputs: pointers into argv, not owned. "-" means stdin. */
    const char *const *files;
    size_t nfiles;

    unsigned xform;     /* OR of enum mat_xform; 0 => fast path eligible */
    bool unbuffered;    /* -u */
    bool stdout_is_tty; /* isatty(STDOUT_FILENO), cached once */

    /* Early-exit actions. */
    bool show_help;
    bool show_version;
};

#endif /* MAT_CONFIG_H */
