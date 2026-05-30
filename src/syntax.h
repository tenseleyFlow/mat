/*
 * syntax.h — resolve an input to a syntax name (the input Sprint 08
 * highlights).
 *
 * Detection order (bat-compatible): explicit -l/--language, then --map-syntax
 * globs, then a built-in extension / whole-name table, then a first-line `#!`
 * shebang, then --fallback-syntax (or "plain"). This sprint only resolves the
 * NAME; no highlighting happens yet.
 */
#ifndef MAT_SYNTAX_H
#define MAT_SYNTAX_H

#include <stddef.h>

struct config;

/*
 * Resolve the syntax name for `name` (a filename, or the --file-name for
 * stdin). `first` / `first_len` are the input's first line (for shebang
 * detection) and may be empty. Returns a static string; never NULL ("plain" if
 * nothing else).
 */
const char *mat_syntax_detect(const struct config *cfg, const char *name,
                              const unsigned char *first, size_t first_len);

#endif /* MAT_SYNTAX_H */
