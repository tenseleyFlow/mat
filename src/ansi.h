/*
 * ansi.h — strip terminal escape sequences from input bytes.
 *
 * Used when decorated/highlighted output would otherwise mix the input's own
 * coloring with mat's. Removes CSI sequences (the SGR color codes), OSC
 * strings, and other ESC-introduced sequences; everything else passes through
 * verbatim.
 */
#ifndef MAT_ANSI_H
#define MAT_ANSI_H

#include <stddef.h>

/* Copy [in,in+n) into out with escape sequences removed; returns the output
 * length. out must have room for n bytes (stripping only ever shrinks). */
size_t mat_strip_ansi(const unsigned char *in, size_t n, char *out);

#endif /* MAT_ANSI_H */
