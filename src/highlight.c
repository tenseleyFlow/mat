#include "highlight.h"

#include <stdlib.h>
#include <string.h>

#define HL_LONG_LINE (16 * 1024) /* past this a line renders unstyled */

typedef int (*lex_fn)(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap);

struct mat_hl {
    lex_fn lex;
    int state; /* lexer-specific carry across lines */
};

/* A dark default theme: token class -> ANSI SGR (empty == default color). */
static const char *const theme_sgr[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[35m",
    [MT_TYPE] = "\x1b[33m",
    [MT_STRING] = "\x1b[32m",
    [MT_NUMBER] = "\x1b[36m",
    [MT_COMMENT] = "\x1b[90m",
    [MT_FUNCTION] = "\x1b[34m",
    [MT_OPERATOR] = "\x1b[36m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[35m",
    [MT_CONSTANT] = "\x1b[33m",
};

const char *mat_theme_sgr(enum mat_tok tok)
{
    if (tok < 0 || tok >= MT_NTOKENS)
        return "";
    return theme_sgr[tok];
}

static int emit(struct mat_span *out, int cap, int n, size_t start, size_t len,
                enum mat_tok tok)
{
    if (len == 0 || n >= cap)
        return n;
    out[n].start = (unsigned)start;
    out[n].len = (unsigned)len;
    out[n].tok = tok;
    return n + 1;
}

static int is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static int is_word(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* ---- JSON ---- */

static int lex_json(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                unsigned char e = d[i++];
                if (e == '"')
                    break;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '-' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_digit(d[i]) || d[i] == '.' || d[i] == 'e' ||
                               d[i] == 'E' || d[i] == '+' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_word(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if ((wl == 4 && memcmp(d + s, "true", 4) == 0) ||
                (wl == 5 && memcmp(d + s, "false", 5) == 0) ||
                (wl == 4 && memcmp(d + s, "null", 4) == 0))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else if (c == '{' || c == '}' || c == '[' || c == ']') {
            i++;
            n = emit(out, cap, n, s, 1, MT_PUNCT);
        } else if (c == ':' || c == ',') {
            i++;
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
        } else {
            i++;
            while (i < len) {
                unsigned char x = d[i];
                if (x == '"' || is_digit(x) || is_word(x) || x == '{' ||
                    x == '}' || x == '[' || x == ']' || x == ':' || x == ',' ||
                    (x == '-' && i + 1 < len && is_digit(d[i + 1])))
                    break;
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_TEXT);
        }
    }
    return n;
}

/* ---- dispatch ---- */

struct mat_hl *mat_hl_open(const char *syntax)
{
    if (syntax == NULL)
        return NULL;
    lex_fn lex = NULL;
    if (strcmp(syntax, "JSON") == 0)
        lex = lex_json;
    if (lex == NULL)
        return NULL;
    struct mat_hl *h = calloc(1, sizeof *h);
    if (h != NULL)
        h->lex = lex;
    return h;
}

void mat_hl_close(struct mat_hl *h)
{
    free(h);
}

int mat_hl_line(struct mat_hl *h, const unsigned char *d, size_t len,
                struct mat_span *out, int cap)
{
    if (cap < 1)
        return 0;
    if (len >= HL_LONG_LINE) { /* long-line guard: one unstyled span */
        out[0].start = 0;
        out[0].len = (unsigned)len;
        out[0].tok = MT_TEXT;
        return 1;
    }
    return h->lex(h, d, len, out, cap);
}
