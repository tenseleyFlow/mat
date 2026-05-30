/*
 * highlight.h — hand-written syntax highlighting (audit 05).
 *
 * A highlighter tokenizes one line at a time into spans; it is stateful so
 * block comments and multi-line strings carry across lines. No asset blob and
 * no regex engine: opening a highlighter costs nothing, so highlight-startup
 * equals the normal startup the tripwire measures. Highlighting is
 * TTY-and-asked-for only; the cat-parity paths never call this.
 */
#ifndef MAT_HIGHLIGHT_H
#define MAT_HIGHLIGHT_H

#include <stddef.h>

/* Token classes the theme colors. */
enum mat_tok {
    MT_TEXT = 0, /* default / whitespace */
    MT_KEYWORD,
    MT_TYPE,
    MT_STRING,
    MT_NUMBER,
    MT_COMMENT,
    MT_FUNCTION,
    MT_OPERATOR,
    MT_PUNCT,
    MT_PREPROC,
    MT_CONSTANT, /* true/false/null and language constants */
    MT_NTOKENS
};

/* A contiguous run of one token class within a line. */
struct mat_span {
    unsigned start; /* byte offset in the line */
    unsigned len;
    enum mat_tok tok;
};

struct mat_hl; /* opaque handle */

/* Open a highlighter for a Sprint-07 syntax name, or NULL if none is available
 * (the caller then renders unstyled). */
struct mat_hl *mat_hl_open(const char *syntax);
void mat_hl_close(struct mat_hl *h);

/*
 * Tokenize one line [d,len) into up to `cap` spans (contiguous, covering the
 * whole line; gaps are MT_TEXT). Returns the span count. Updates state for the
 * next line. A line at/over the long-line guard returns a single MT_TEXT span.
 */
int mat_hl_line(struct mat_hl *h, const unsigned char *d, size_t len,
                struct mat_span *spans, int cap);

/* The ANSI SGR introducing a token class (empty string for MT_TEXT). */
const char *mat_theme_sgr(enum mat_tok tok);

#endif /* MAT_HIGHLIGHT_H */
