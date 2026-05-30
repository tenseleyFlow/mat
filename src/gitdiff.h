/*
 * gitdiff.h — per-file git change markers for the decoration gutter.
 *
 * Runs `git diff` lazily (only when -d or style=changes is active) and parses
 * the unified diff to classify each line as added, modified, removed-after, or
 * unchanged. The subprocess is never spawned unless markers are requested.
 */
#ifndef MAT_GITDIFF_H
#define MAT_GITDIFF_H

#include <stdbool.h>
#include <stddef.h>

enum mat_change {
    MAT_CHG_NONE = 0,
    MAT_CHG_ADDED,    /* new line (+ in the diff) */
    MAT_CHG_MODIFIED, /* changed line (context heuristic) */
    MAT_CHG_REMOVED,  /* a line was removed after this one */
};

struct mat_changes {
    enum mat_change *line; /* 1-indexed: line[1..n] */
    size_t cap;
    size_t nlines;
};

/* Run `git diff` for `path` and populate `ch`. Returns false if git is not
 * available, the file is not tracked, or the diff failed (ch is zeroed).
 * The caller must call mat_changes_free. */
bool mat_changes_load(struct mat_changes *ch, const char *path);

void mat_changes_free(struct mat_changes *ch);

/* The single-character gutter marker for a change class. */
const char *mat_change_marker(enum mat_change c);

/* The ANSI color for a change marker (green/yellow/red). */
const char *mat_change_color(enum mat_change c);

#endif /* MAT_GITDIFF_H */
