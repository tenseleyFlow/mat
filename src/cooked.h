/*
 * cooked.h — the transforming path (-n -b -s -A -v -e -t -E -T).
 *
 * Reached only when transform flags are set (the cooked branch of the
 * TTY/pipe split). Byte-oriented, buffered, and byte-for-byte compatible with
 * GNU cat's output. State (line number, squeeze, pending CR) persists across
 * the concatenated inputs, exactly like cat.
 */
#ifndef MAT_COOKED_H
#define MAT_COOKED_H

#include "config.h"

void mat_cooked_run(const struct config *cfg);

#endif /* MAT_COOKED_H */
