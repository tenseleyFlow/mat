#include "highlight.h"

#include <stdlib.h>
#include <string.h>

#define HL_LONG_LINE (16 * 1024) /* past this a line renders unstyled */

typedef int (*lex_fn)(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap);

struct wordset {
    const char *const *words;
    int n;
};

struct mat_hl {
    lex_fn lex;
    int state; /* 0=normal, 1=block-comment, 2=multi-line-string */
    struct wordset keywords;
    struct wordset types;
};

#include <stdio.h>

/* Named themes: each maps token class -> ANSI SGR (empty == default). */
static const char *const theme_dark[MT_NTOKENS] = {
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

static const char *const theme_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[35m",
    [MT_TYPE] = "\x1b[34m",
    [MT_STRING] = "\x1b[31m",
    [MT_NUMBER] = "\x1b[36m",
    [MT_COMMENT] = "\x1b[37m",
    [MT_FUNCTION] = "\x1b[34m",
    [MT_OPERATOR] = "\x1b[36m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[35m",
    [MT_CONSTANT] = "\x1b[34m",
};

static const char *const theme_monokai[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;197m",
    [MT_TYPE] = "\x1b[38;5;81m",
    [MT_STRING] = "\x1b[38;5;186m",
    [MT_NUMBER] = "\x1b[38;5;141m",
    [MT_COMMENT] = "\x1b[38;5;242m",
    [MT_FUNCTION] = "\x1b[38;5;148m",
    [MT_OPERATOR] = "\x1b[38;5;197m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;197m",
    [MT_CONSTANT] = "\x1b[38;5;141m",
};

static const char *const theme_dracula[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;212m",
    [MT_TYPE] = "\x1b[38;5;159m",
    [MT_STRING] = "\x1b[38;5;229m",
    [MT_NUMBER] = "\x1b[38;5;183m",
    [MT_COMMENT] = "\x1b[38;5;103m",
    [MT_FUNCTION] = "\x1b[38;5;120m",
    [MT_OPERATOR] = "\x1b[38;5;212m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;212m",
    [MT_CONSTANT] = "\x1b[38;5;183m",
};

static const char *const theme_solarized_dark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;142m",
    [MT_TYPE] = "\x1b[38;5;178m",
    [MT_STRING] = "\x1b[38;5;73m",
    [MT_NUMBER] = "\x1b[38;5;73m",
    [MT_COMMENT] = "\x1b[38;5;102m",
    [MT_FUNCTION] = "\x1b[38;5;74m",
    [MT_OPERATOR] = "\x1b[38;5;142m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;166m",
    [MT_CONSTANT] = "\x1b[38;5;73m",
};

static const char *const theme_solarized_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;142m",
    [MT_TYPE] = "\x1b[38;5;178m",
    [MT_STRING] = "\x1b[38;5;73m",
    [MT_NUMBER] = "\x1b[38;5;73m",
    [MT_COMMENT] = "\x1b[38;5;247m",
    [MT_FUNCTION] = "\x1b[38;5;74m",
    [MT_OPERATOR] = "\x1b[38;5;142m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;166m",
    [MT_CONSTANT] = "\x1b[38;5;73m",
};

static const char *const theme_nord[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;146m",
    [MT_TYPE] = "\x1b[38;5;146m",
    [MT_STRING] = "\x1b[38;5;151m",
    [MT_NUMBER] = "\x1b[38;5;248m",
    [MT_COMMENT] = "\x1b[38;5;103m",
    [MT_FUNCTION] = "\x1b[38;5;152m",
    [MT_OPERATOR] = "\x1b[38;5;146m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;146m",
    [MT_CONSTANT] = "\x1b[38;5;254m",
};

static const char *const theme_gruvbox[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;203m",
    [MT_TYPE] = "\x1b[38;5;221m",
    [MT_STRING] = "\x1b[38;5;185m",
    [MT_NUMBER] = "\x1b[38;5;181m",
    [MT_COMMENT] = "\x1b[38;5;244m",
    [MT_FUNCTION] = "\x1b[38;5;185m",
    [MT_OPERATOR] = "\x1b[38;5;223m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;150m",
    [MT_CONSTANT] = "\x1b[38;5;181m",
};

static const char *const theme_onedark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",
    [MT_TYPE] = "\x1b[38;5;186m",
    [MT_STRING] = "\x1b[38;5;150m",
    [MT_NUMBER] = "\x1b[38;5;180m",
    [MT_COMMENT] = "\x1b[38;5;102m",
    [MT_FUNCTION] = "\x1b[38;5;111m",
    [MT_OPERATOR] = "\x1b[38;5;116m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;176m",
    [MT_CONSTANT] = "\x1b[38;5;180m",
};

static const char *const theme_catppuccin[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;183m",
    [MT_TYPE] = "\x1b[38;5;223m",
    [MT_STRING] = "\x1b[38;5;151m",
    [MT_NUMBER] = "\x1b[38;5;223m",
    [MT_COMMENT] = "\x1b[38;5;247m",
    [MT_FUNCTION] = "\x1b[38;5;153m",
    [MT_OPERATOR] = "\x1b[38;5;153m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;225m",
    [MT_CONSTANT] = "\x1b[38;5;223m",
};

static const char *const theme_github_dark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;204m",
    [MT_TYPE] = "\x1b[38;5;215m",
    [MT_STRING] = "\x1b[38;5;116m",
    [MT_NUMBER] = "\x1b[38;5;116m",
    [MT_COMMENT] = "\x1b[38;5;243m",
    [MT_FUNCTION] = "\x1b[38;5;183m",
    [MT_OPERATOR] = "\x1b[38;5;204m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;204m",
    [MT_CONSTANT] = "\x1b[38;5;116m",
};

static const char *const theme_github_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;161m",
    [MT_TYPE] = "\x1b[38;5;130m",
    [MT_STRING] = "\x1b[38;5;28m",
    [MT_NUMBER] = "\x1b[38;5;28m",
    [MT_COMMENT] = "\x1b[38;5;245m",
    [MT_FUNCTION] = "\x1b[38;5;99m",
    [MT_OPERATOR] = "\x1b[38;5;161m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;161m",
    [MT_CONSTANT] = "\x1b[38;5;28m",
};

static const char *const theme_tokyonight[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",
    [MT_TYPE] = "\x1b[38;5;116m",
    [MT_STRING] = "\x1b[38;5;150m",
    [MT_NUMBER] = "\x1b[38;5;215m",
    [MT_COMMENT] = "\x1b[38;5;60m",
    [MT_FUNCTION] = "\x1b[38;5;75m",
    [MT_OPERATOR] = "\x1b[38;5;116m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;176m",
    [MT_CONSTANT] = "\x1b[38;5;215m",
};

static const char *const *active_theme = theme_dark;

int mat_theme_set(const char *name)
{
    if (name == NULL || strcmp(name, "dark") == 0)
        active_theme = theme_dark;
    else if (strcmp(name, "light") == 0)
        active_theme = theme_light;
    else if (strcmp(name, "monokai") == 0)
        active_theme = theme_monokai;
    else if (strcmp(name, "dracula") == 0)
        active_theme = theme_dracula;
    else if (strcmp(name, "solarized-dark") == 0)
        active_theme = theme_solarized_dark;
    else if (strcmp(name, "solarized-light") == 0)
        active_theme = theme_solarized_light;
    else if (strcmp(name, "nord") == 0)
        active_theme = theme_nord;
    else if (strcmp(name, "gruvbox") == 0)
        active_theme = theme_gruvbox;
    else if (strcmp(name, "onedark") == 0)
        active_theme = theme_onedark;
    else if (strcmp(name, "catppuccin") == 0)
        active_theme = theme_catppuccin;
    else if (strcmp(name, "github-dark") == 0)
        active_theme = theme_github_dark;
    else if (strcmp(name, "github-light") == 0)
        active_theme = theme_github_light;
    else if (strcmp(name, "tokyonight") == 0)
        active_theme = theme_tokyonight;
    else
        return -1;
    return 0;
}

const char *mat_theme_sgr(enum mat_tok tok)
{
    if (tok < 0 || tok >= MT_NTOKENS)
        return "";
    return active_theme[tok];
}

void mat_theme_list(void)
{
    printf("catppuccin\ndark\ndracula\ngithub-dark\ngithub-light\n"
           "gruvbox\nlight\nmonokai\nnord\nonedark\n"
           "solarized-dark\nsolarized-light\ntokyonight\n");
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

static int ws_has(const struct wordset *ws, const char *w, size_t wl)
{
    for (int i = 0; i < ws->n; i++)
        if (strlen(ws->words[i]) == wl && memcmp(ws->words[i], w, wl) == 0)
            return 1;
    return 0;
}

static int is_alnum(unsigned char c)
{
    return is_word(c) || is_digit(c);
}

/* ---- C-family: C, C++, Java, JavaScript, Go, Rust ---- */

enum { HL_NORMAL = 0, HL_BLOCK_COMMENT = 1 };

static int lex_cfamily(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '/' && i + 1 < len && d[i + 1] == '/') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_PREPROC);
            return n;
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == 'x' ||
                               d[i] == 'X' || d[i] == '+' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
            n = emit(out, cap, n, s, 1,
                     (c == '(' || c == ')' || c == '{' || c == '}' ||
                      c == '[' || c == ']' || c == ';')
                         ? MT_PUNCT
                         : MT_TEXT);
        }
    }
    return n;
}

/* ---- Python ---- */

enum { HL_PY_TRIPLE = 2 };

static int lex_python(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_PY_TRIPLE) {
        size_t s = 0;
        while (i + 2 < len) {
            if (d[i] == '"' && d[i + 1] == '"' && d[i + 2] == '"') {
                i += 3;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_PY_TRIPLE)
            i = len;
        n = emit(out, cap, n, s, i, MT_STRING);
        if (h->state == HL_PY_TRIPLE)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if ((c == '"' || c == '\'') && i + 2 < len && d[i + 1] == c &&
            d[i + 2] == c) {
            h->state = HL_PY_TRIPLE;
            i += 3;
            while (i + 2 < len) {
                if (d[i] == c && d[i + 1] == c && d[i + 2] == c) {
                    i += 3;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_PY_TRIPLE)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (c == '@') {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Shell (Bash/Zsh) ---- */

static int lex_shell(struct mat_hl *h, const unsigned char *d, size_t len,
                     struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (q == '"' && d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '$') {
            i++;
            if (i < len && d[i] == '{') {
                while (i < len && d[i] != '}')
                    i++;
                if (i < len)
                    i++;
            } else {
                while (i < len && is_alnum(d[i]))
                    i++;
            }
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_digit(c)) {
            i++;
            while (i < len && is_digit(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
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

/* ---- Fortran (free-form; ! comments, case-insensitive keywords) ---- */

static int ci_match(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb + 32);
        if (ca != cb)
            return 0;
    }
    return 1;
}

static int ws_has_ci(const struct wordset *ws, const char *w, size_t wl)
{
    for (int i = 0; i < ws->n; i++)
        if (strlen(ws->words[i]) == wl && ci_match(ws->words[i], w, wl))
            return 1;
    return 0;
}

static int lex_fortran(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '!') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '\'' || c == '"') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == q) {
                    i++;
                    if (i < len && d[i] == q) {
                        i++;
                        continue;
                    }
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '+' ||
                               d[i] == '-' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has_ci(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has_ci(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Markdown ---- */

static int lex_markdown(struct mat_hl *h, const unsigned char *d, size_t len,
                        struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        if (len >= 3 && d[0] == '`' && d[1] == '`' && d[2] == '`') {
            h->state = HL_NORMAL;
            n = emit(out, cap, n, 0, len, MT_PREPROC);
            return n;
        }
        n = emit(out, cap, n, s, len, MT_STRING);
        return n;
    }
    if (len >= 3 && d[0] == '`' && d[1] == '`' && d[2] == '`') {
        h->state = HL_BLOCK_COMMENT;
        n = emit(out, cap, n, 0, len, MT_PREPROC);
        return n;
    }
    if (len > 0 && d[0] == '#') {
        n = emit(out, cap, n, 0, len, MT_KEYWORD);
        return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '`') {
            i++;
            while (i < len && d[i] != '`')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '[') {
            i++;
            while (i < len && d[i] != ']')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_FUNCTION);
            if (i < len && d[i] == '(') {
                size_t ls = i;
                i++;
                while (i < len && d[i] != ')')
                    i++;
                if (i < len)
                    i++;
                n = emit(out, cap, n, ls, i - ls, MT_PREPROC);
            }
        } else if ((c == '*' || c == '_') && i + 1 < len && d[i + 1] == c) {
            unsigned char q = c;
            i += 2;
            while (i + 1 < len && !(d[i] == q && d[i + 1] == q))
                i++;
            if (i + 1 < len)
                i += 2;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (c == '*' || c == '_') {
            i++;
            while (i < len && d[i] != c)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_TYPE);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- YAML ---- */

static int lex_yaml(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len >= 3 && d[0] == '-' && d[1] == '-' && d[2] == '-') {
        n = emit(out, cap, n, 0, len, MT_OPERATOR);
        return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == ':' && (i + 1 >= len || d[i + 1] == ' ')) {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (c == '-' && (i + 1 >= len || d[i + 1] == ' ')) {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (i < len && d[i] == ':')
                t = MT_KEYWORD;
            else if ((wl == 4 && ci_match("true", (const char *)d + s, 4)) ||
                     (wl == 5 && ci_match("false", (const char *)d + s, 5)) ||
                     (wl == 4 && ci_match("null", (const char *)d + s, 4)))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_digit(d[i]) || d[i] == '.' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- TOML ---- */

static int lex_toml(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '[') {
            i++;
            while (i < len && d[i] != ']')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '=') {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '-' ||
                               d[i] == ':' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if ((wl == 4 && memcmp(d + s, "true", 4) == 0) ||
                (wl == 5 && memcmp(d + s, "false", 5) == 0))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- HTML ---- */

static int lex_html(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 2 < len) {
            if (d[i] == '-' && d[i + 1] == '-' && d[i + 2] == '>') {
                i += 3;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        if (d[i] == '<' && i + 3 < len && d[i + 1] == '!' && d[i + 2] == '-' &&
            d[i + 3] == '-') {
            h->state = HL_BLOCK_COMMENT;
            i += 4;
            while (i + 2 < len) {
                if (d[i] == '-' && d[i + 1] == '-' && d[i + 2] == '>') {
                    i += 3;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (d[i] == '<') {
            i++;
            if (i < len && d[i] == '/')
                i++;
            size_t ts = i;
            while (i < len && is_alnum(d[i]))
                i++;
            if (i > ts)
                n = emit(out, cap, n, s, i - s, MT_KEYWORD);
            while (i < len && d[i] != '>') {
                if (d[i] == '"' || d[i] == '\'') {
                    size_t qs = i;
                    unsigned char q = d[i++];
                    while (i < len && d[i] != q)
                        i++;
                    if (i < len)
                        i++;
                    n = emit(out, cap, n, qs, i - qs, MT_STRING);
                } else if (is_word(d[i])) {
                    size_t as = i;
                    while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                        i++;
                    n = emit(out, cap, n, as, i - as, MT_TYPE);
                } else {
                    i++;
                }
            }
            if (i < len) {
                n = emit(out, cap, n, i, 1, MT_KEYWORD);
                i++;
            }
        } else if (d[i] == '&') {
            i++;
            while (i < len && d[i] != ';')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_CONSTANT);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- CSS ---- */

static int lex_css(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '#' || c == '.') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_FUNCTION);
        } else if (c == '@') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (c == ':') {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (c == '{' || c == '}' || c == ';') {
            n = emit(out, cap, n, s, 1, MT_PUNCT);
            i++;
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '%'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c) || c == '-') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_TEXT);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- SQL ---- */

static int lex_sql(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '-' && i + 1 < len && d[i + 1] == '-') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '\'') {
            i++;
            while (i < len) {
                if (d[i] == '\'' && i + 1 < len && d[i + 1] == '\'') {
                    i += 2;
                    continue;
                }
                if (d[i] == '\'') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_digit(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has_ci(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has_ci(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if ((wl == 4 && ci_match("null", (const char *)d + s, 4)) ||
                     (wl == 4 && ci_match("true", (const char *)d + s, 4)) ||
                     (wl == 5 && ci_match("false", (const char *)d + s, 5)))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Ruby ---- */

static int lex_ruby(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (q == '"' && d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == ':' && i + 1 < len && is_word(d[i + 1])) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_CONSTANT);
        } else if (c == '@') {
            i++;
            if (i < len && d[i] == '@')
                i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '?' ||
                               d[i] == '!'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && (d[i] == '(' || d[i] == ' '))
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Lua ---- */

static int lex_lua(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == ']' && d[i + 1] == ']') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '-' && i + 1 < len && d[i + 1] == '-') {
            if (i + 3 < len && d[i + 2] == '[' && d[i + 3] == '[') {
                h->state = HL_BLOCK_COMMENT;
                i += 4;
                while (i + 1 < len) {
                    if (d[i] == ']' && d[i + 1] == ']') {
                        i += 2;
                        h->state = HL_NORMAL;
                        break;
                    }
                    i++;
                }
                if (h->state == HL_BLOCK_COMMENT)
                    i = len;
            } else {
                i = len;
            }
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && (d[i] == '(' || d[i] == '.'))
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Makefile ---- */

static int lex_makefile(struct mat_hl *h, const unsigned char *d, size_t len,
                        struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len > 0 && d[0] == '\t') {
        n = emit(out, cap, n, 0, len, MT_STRING);
        return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '$') {
            i++;
            if (i < len && (d[i] == '(' || d[i] == '{')) {
                unsigned char close = (unsigned char)(d[i] == '(' ? ')' : '}');
                while (i < len && d[i] != close)
                    i++;
                if (i < len)
                    i++;
            } else {
                if (i < len)
                    i++;
            }
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (c == ':' || c == '=' || c == '?') {
            i++;
            if (i < len && d[i] == '=')
                i++;
            n = emit(out, cap, n, s, i - s, MT_OPERATOR);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-' ||
                               d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_TEXT);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Diff ---- */

static int lex_diff(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    if (len == 0)
        return 0;
    enum mat_tok t;
    if (d[0] == '+')
        t = MT_STRING;
    else if (d[0] == '-')
        t = MT_KEYWORD;
    else if (d[0] == '@')
        t = MT_PREPROC;
    else if (len >= 4 &&
             (memcmp(d, "diff", 4) == 0 || memcmp(d, "index", 5) == 0))
        t = MT_FUNCTION;
    else
        t = MT_TEXT;
    return emit(out, cap, 0, 0, len, t);
}

/* ---- keyword tables ---- */

static const char *const c_kw[] = {
    "auto",     "break",     "case",           "continue", "default",
    "do",       "else",      "enum",           "extern",   "for",
    "goto",     "if",        "inline",         "register", "restrict",
    "return",   "sizeof",    "static",         "struct",   "switch",
    "typedef",  "union",     "volatile",       "while",    "_Alignas",
    "_Alignof", "_Noreturn", "_Static_assert",
};
static const char *const c_ty[] = {
    "void",     "char",     "short",    "int",     "long",    "float",
    "double",   "signed",   "unsigned", "bool",    "size_t",  "ssize_t",
    "int8_t",   "int16_t",  "int32_t",  "int64_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t", "FILE",     "NULL",
};
static const char *const py_kw[] = {
    "and",      "as",       "assert", "async", "await",  "break",  "class",
    "continue", "def",      "del",    "elif",  "else",   "except", "finally",
    "for",      "from",     "global", "if",    "import", "in",     "is",
    "lambda",   "nonlocal", "not",    "or",    "pass",   "raise",  "return",
    "try",      "while",    "with",   "yield",
};
static const char *const py_ty[] = {
    "True", "False", "None",  "int",  "float", "str",  "list",
    "dict", "set",   "tuple", "bool", "bytes", "type", "self",
};
static const char *const sh_kw[] = {
    "if",       "then",   "else",     "elif",    "fi",    "for",
    "while",    "do",     "done",     "case",    "esac",  "in",
    "function", "select", "until",    "return",  "break", "continue",
    "local",    "export", "readonly", "declare",
};
static const char *const js_kw[] = {
    "break",    "case",       "catch",  "class",    "const", "continue",
    "debugger", "default",    "delete", "do",       "else",  "export",
    "extends",  "finally",    "for",    "function", "if",    "import",
    "in",       "instanceof", "let",    "new",      "of",    "return",
    "super",    "switch",     "this",   "throw",    "try",   "typeof",
    "var",      "void",       "while",  "with",     "yield", "async",
    "await",
};
static const char *const js_ty[] = {
    "true",   "false",   "null",   "undefined", "NaN", "Infinity", "Number",
    "String", "Boolean", "Object", "Array",     "Map", "Set",
};
static const char *const go_kw[] = {
    "break",  "case",        "chan", "const",   "continue", "default", "defer",
    "else",   "fallthrough", "for",  "func",    "go",       "goto",    "if",
    "import", "interface",   "map",  "package", "range",    "return",  "select",
    "struct", "switch",      "type", "var",
};
static const char *const go_ty[] = {
    "bool",    "byte",    "complex64", "complex128", "error",  "float32",
    "float64", "int",     "int8",      "int16",      "int32",  "int64",
    "rune",    "string",  "uint",      "uint8",      "uint16", "uint32",
    "uint64",  "uintptr", "true",      "false",      "nil",    "iota",
};
static const char *const rs_kw[] = {
    "as",     "async", "await", "break",  "const",  "continue", "crate",
    "dyn",    "else",  "enum",  "extern", "fn",     "for",      "if",
    "impl",   "in",    "let",   "loop",   "match",  "mod",      "move",
    "mut",    "pub",   "ref",   "return", "self",   "Self",     "static",
    "struct", "super", "trait", "type",   "unsafe", "use",      "where",
    "while",  "yield",
};
static const char *const rs_ty[] = {
    "bool",   "char", "f32",   "f64",    "i8",  "i16",  "i32",
    "i64",    "i128", "isize", "str",    "u8",  "u16",  "u32",
    "u64",    "u128", "usize", "String", "Vec", "Box",  "Option",
    "Result", "Some", "None",  "Ok",     "Err", "true", "false",
};

static const char *const fortran_kw[] = {
    "program",   "end",      "subroutine", "function",   "module",
    "use",       "implicit", "none",       "call",       "return",
    "if",        "then",     "else",       "elseif",     "endif",
    "do",        "while",    "enddo",      "select",     "case",
    "where",     "forall",   "continue",   "stop",       "exit",
    "cycle",     "goto",     "allocate",   "deallocate", "contains",
    "interface", "type",     "class",      "associate",  "block",
    "data",      "save",     "common",     "intent",     "in",
    "out",       "inout",    "optional",   "recursive",  "pure",
    "elemental", "abstract",
};
static const char *const fortran_ty[] = {
    "integer",   "real",    "double",    "precision", "complex",
    "character", "logical", "dimension", "parameter", "allocatable",
    "pointer",   "target",  "kind",
};
static const char *const sql_kw[] = {
    "select",  "from",    "where",      "and",        "or",       "not",
    "insert",  "into",    "values",     "update",     "set",      "delete",
    "create",  "drop",    "alter",      "table",      "index",    "view",
    "join",    "inner",   "outer",      "left",       "right",    "on",
    "group",   "by",      "order",      "having",     "limit",    "offset",
    "union",   "all",     "distinct",   "as",         "exists",   "in",
    "between", "like",    "is",         "case",       "when",     "then",
    "else",    "end",     "begin",      "commit",     "rollback", "primary",
    "key",     "foreign", "references", "constraint", "default",  "with",
};
static const char *const sql_ty[] = {
    "int",     "integer", "bigint", "smallint",  "varchar", "char",
    "text",    "boolean", "date",   "timestamp", "float",   "double",
    "decimal", "numeric", "blob",   "serial",    "uuid",
};
static const char *const ruby_kw[] = {
    "def",     "end",    "class",  "module", "if",     "unless", "elsif",
    "else",    "case",   "when",   "while",  "until",  "for",    "do",
    "begin",   "rescue", "ensure", "raise",  "return", "yield",  "require",
    "include", "extend", "puts",   "print",  "lambda", "proc",
};
static const char *const ruby_ty[] = {
    "true",  "false", "nil",  "self",   "String", "Integer",
    "Float", "Array", "Hash", "Symbol", "Proc",   "Class",
};
static const char *const lua_kw[] = {
    "and",      "break",  "do",   "else",  "elseif", "end", "for",
    "function", "goto",   "if",   "in",    "local",  "not", "or",
    "repeat",   "return", "then", "until", "while",
};
static const char *const lua_ty[] = {
    "true",
    "false",
    "nil",
};
static const char *const cs_kw[] = {
    "abstract",  "as",        "base",     "bool",     "break",    "case",
    "catch",     "class",     "const",    "continue", "default",  "delegate",
    "do",        "else",      "enum",     "event",    "explicit", "extern",
    "finally",   "for",       "foreach",  "goto",     "if",       "implicit",
    "in",        "interface", "internal", "is",       "lock",     "namespace",
    "new",       "operator",  "out",      "override", "params",   "private",
    "protected", "public",    "readonly", "ref",      "return",   "sealed",
    "static",    "struct",    "switch",   "this",     "throw",    "try",
    "typeof",    "using",     "var",      "virtual",  "void",     "volatile",
    "while",     "yield",     "async",    "await",
};
static const char *const cs_ty[] = {
    "int",   "long",   "short",  "byte",  "float",  "double", "decimal",
    "char",  "string", "object", "bool",  "uint",   "ulong",  "ushort",
    "sbyte", "null",   "true",   "false", "String", "Int32",
};
static const char *const kt_kw[] = {
    "abstract", "as",          "break",    "by",        "class",    "companion",
    "const",    "constructor", "continue", "data",      "do",       "else",
    "enum",     "false",       "finally",  "for",       "fun",      "if",
    "import",   "in",          "init",     "interface", "internal", "is",
    "lateinit", "null",        "object",   "open",      "operator", "out",
    "override", "package",     "private",  "protected", "public",   "return",
    "sealed",   "super",       "suspend",  "this",      "throw",    "true",
    "try",      "typealias",   "val",      "var",       "when",     "while",
};
static const char *const kt_ty[] = {
    "Any", "Boolean", "Byte",    "Char",  "Double", "Float",
    "Int", "Long",    "Nothing", "Short", "String", "Unit",
};
static const char *const scala_kw[] = {
    "abstract",  "case",    "catch",    "class",    "def",     "do",
    "else",      "extends", "false",    "final",    "finally", "for",
    "forSome",   "if",      "implicit", "import",   "lazy",    "match",
    "new",       "null",    "object",   "override", "package", "private",
    "protected", "return",  "sealed",   "super",    "this",    "throw",
    "trait",     "true",    "try",      "type",     "val",     "var",
    "while",     "with",    "yield",
};
static const char *const scala_ty[] = {
    "Any", "AnyRef", "AnyVal",  "Boolean", "Byte",  "Char",   "Double", "Float",
    "Int", "Long",   "Nothing", "Null",    "Short", "String", "Unit",
};
static const char *const swift_kw[] = {
    "associatedtype",
    "break",
    "case",
    "catch",
    "class",
    "continue",
    "default",
    "defer",
    "deinit",
    "do",
    "else",
    "enum",
    "extension",
    "fallthrough",
    "fileprivate",
    "for",
    "func",
    "guard",
    "if",
    "import",
    "in",
    "init",
    "inout",
    "internal",
    "is",
    "let",
    "open",
    "operator",
    "private",
    "protocol",
    "public",
    "repeat",
    "rethrows",
    "return",
    "self",
    "static",
    "struct",
    "subscript",
    "super",
    "switch",
    "throw",
    "throws",
    "try",
    "typealias",
    "var",
    "where",
    "while",
};
static const char *const swift_ty[] = {
    "Any",    "Bool",   "Character", "Double", "Float",  "Int",
    "Int8",   "Int16",  "Int32",     "Int64",  "Never",  "Optional",
    "Self",   "String", "UInt",      "UInt8",  "UInt16", "UInt32",
    "UInt64", "Void",   "true",      "false",  "nil",
};
static const char *const dart_kw[] = {
    "abstract", "as",        "assert",   "async",      "await",    "break",
    "case",     "catch",     "class",    "const",      "continue", "default",
    "deferred", "do",        "dynamic",  "else",       "enum",     "export",
    "extends",  "extension", "external", "factory",    "final",    "finally",
    "for",      "get",       "if",       "implements", "import",   "in",
    "is",       "late",      "library",  "mixin",      "new",      "on",
    "operator", "part",      "required", "rethrow",    "return",   "sealed",
    "set",      "show",      "static",   "super",      "switch",   "sync",
    "this",     "throw",     "try",      "typedef",    "var",      "void",
    "while",    "with",      "yield",
};
static const char *const dart_ty[] = {
    "bool",   "double", "dynamic", "int",   "num",  "Object",
    "String", "void",   "List",    "Map",   "Set",  "Future",
    "Stream", "null",   "true",    "false", "Null",
};
static const char *const perl_kw[] = {
    "chomp",   "chop",   "die",     "do",      "else",   "elsif",
    "eval",    "for",    "foreach", "given",   "goto",   "if",
    "last",    "local",  "my",      "next",    "no",     "our",
    "package", "print",  "redo",    "require", "return", "say",
    "sub",     "unless", "until",   "use",     "when",   "while",
};
static const char *const php_kw[] = {
    "abstract",  "and",        "as",        "break",      "case",
    "catch",     "class",      "clone",     "const",      "continue",
    "declare",   "default",    "do",        "echo",       "else",
    "elseif",    "empty",      "endfor",    "endforeach", "endif",
    "endswitch", "endwhile",   "eval",      "exit",       "extends",
    "final",     "finally",    "fn",        "for",        "foreach",
    "function",  "global",     "goto",      "if",         "implements",
    "include",   "instanceof", "interface", "isset",      "list",
    "match",     "namespace",  "new",       "or",         "print",
    "private",   "protected",  "public",    "readonly",   "require",
    "return",    "static",     "switch",    "this",       "throw",
    "trait",     "try",        "unset",     "use",        "var",
    "while",     "xor",        "yield",
};
static const char *const php_ty[] = {
    "array", "bool",  "callable", "float", "int",    "iterable",
    "mixed", "null",  "object",   "self",  "string", "void",
    "true",  "false", "NULL",     "TRUE",  "FALSE",
};
static const char *const haskell_kw[] = {
    "case",    "class",  "data",      "default",  "deriving", "do",
    "else",    "forall", "foreign",   "if",       "import",   "in",
    "infix",   "infixl", "infixr",    "instance", "let",      "module",
    "newtype", "of",     "qualified", "then",     "type",     "where",
};
static const char *const haskell_ty[] = {
    "Bool", "Char",    "Double", "Either", "Float", "IO",
    "Int",  "Integer", "Maybe",  "String", "True",  "False",
    "Just", "Nothing", "Left",   "Right",
};
static const char *const elixir_kw[] = {
    "after",     "alias",   "and",      "case",      "catch", "cond",
    "def",       "defimpl", "defmacro", "defmodule", "defp",  "defprotocol",
    "defstruct", "do",      "else",     "end",       "fn",    "for",
    "if",        "import",  "in",       "not",       "or",    "quote",
    "raise",     "receive", "require",  "rescue",    "try",   "unless",
    "unquote",   "use",     "when",     "with",
};
static const char *const elixir_ty[] = {
    "true",
    "false",
    "nil",
    "self",
};
static const char *const erlang_kw[] = {
    "after", "and",    "andalso", "begin", "case", "catch",
    "end",   "fun",    "if",      "let",   "not",  "of",
    "or",    "orelse", "receive", "try",   "when",
};
static const char *const erlang_ty[] = {
    "true",
    "false",
    "undefined",
};
static const char *const r_kw[] = {
    "break",  "else",   "for",   "function", "if",      "in",     "next",
    "repeat", "return", "while", "library",  "require", "source",
};
static const char *const r_ty[] = {
    "TRUE", "FALSE",       "NULL",     "NA",          "Inf",
    "NaN",  "NA_integer_", "NA_real_", "NA_complex_", "NA_character_",
};
static const char *const zig_kw[] = {
    "addrspace", "align",  "and",      "asm",       "break",       "catch",
    "comptime",  "const",  "continue", "defer",     "else",        "enum",
    "errdefer",  "error",  "export",   "extern",    "fn",          "for",
    "if",        "inline", "noalias",  "nosuspend", "or",          "orelse",
    "packed",    "pub",    "resume",   "return",    "struct",      "suspend",
    "switch",    "test",   "try",      "union",     "unreachable", "var",
    "volatile",  "while",
};
static const char *const zig_ty[] = {
    "bool", "f16",   "f32",  "f64",       "f80",  "f128", "i8",    "i16",
    "i32",  "i64",   "i128", "isize",     "u8",   "u16",  "u32",   "u64",
    "u128", "usize", "void", "anyopaque", "null", "true", "false", "undefined",
};
static const char *const ocaml_kw[] = {
    "and",      "as",      "assert",  "begin",       "class",   "constraint",
    "do",       "done",    "downto",  "else",        "end",     "exception",
    "external", "for",     "fun",     "function",    "functor", "if",
    "in",       "include", "inherit", "initializer", "lazy",    "let",
    "match",    "method",  "module",  "mutable",     "new",     "object",
    "of",       "open",    "or",      "private",     "rec",     "sig",
    "struct",   "then",    "to",      "try",         "type",    "val",
    "virtual",  "when",    "while",   "with",
};
static const char *const ocaml_ty[] = {
    "int",   "float",  "bool", "char", "string", "unit", "list",
    "array", "option", "ref",  "true", "false",  "None", "Some",
};
static const char *const clojure_kw[] = {
    "def",     "defn", "defmacro", "defonce", "fn",    "if",    "do",
    "let",     "loop", "recur",    "throw",   "try",   "catch", "finally",
    "cond",    "case", "when",     "and",     "or",    "not",   "ns",
    "require", "use",  "import",   "in-ns",   "refer",
};
static const char *const clojure_ty[] = {
    "nil",
    "true",
    "false",
};
static const char *const julia_kw[] = {
    "abstract",  "baremodule", "begin",    "break",  "catch",  "const",
    "continue",  "do",         "else",     "elseif", "end",    "export",
    "finally",   "for",        "function", "global", "if",     "import",
    "in",        "let",        "local",    "macro",  "module", "mutable",
    "primitive", "quote",      "return",   "struct", "try",    "type",
    "using",     "where",      "while",
};
static const char *const julia_ty[] = {
    "Any",     "Bool",    "Char",  "Float16", "Float32", "Float64",
    "Int",     "Int8",    "Int16", "Int32",   "Int64",   "Int128",
    "Nothing", "String",  "UInt",  "UInt8",   "UInt16",  "UInt32",
    "UInt64",  "UInt128", "true",  "false",   "nothing",
};
static const char *const nim_kw[] = {
    "addr",      "and",     "as",        "asm",      "bind",   "block",
    "break",     "case",    "cast",      "concept",  "const",  "continue",
    "converter", "defer",   "discard",   "distinct", "div",    "do",
    "elif",      "else",    "end",       "enum",     "except", "export",
    "finally",   "for",     "from",      "func",     "if",     "import",
    "in",        "include", "interface", "is",       "isnot",  "iterator",
    "let",       "macro",   "method",    "mixin",    "mod",    "nil",
    "not",       "notin",   "object",    "of",       "or",     "out",
    "proc",      "ptr",     "raise",     "ref",      "return", "shl",
    "shr",       "static",  "template",  "try",      "tuple",  "type",
    "using",     "var",     "when",      "while",    "xor",    "yield",
};
static const char *const nim_ty[] = {
    "int",     "int8",   "int16",  "int32",  "int64", "uint",
    "uint8",   "uint16", "uint32", "uint64", "float", "float32",
    "float64", "bool",   "char",   "string", "seq",   "array",
    "void",    "true",   "false",  "nil",
};
static const char *const groovy_kw[] = {
    "abstract",     "as",     "assert",     "break",    "case",    "catch",
    "class",        "const",  "continue",   "def",      "default", "do",
    "else",         "enum",   "extends",    "final",    "finally", "for",
    "goto",         "if",     "implements", "import",   "in",      "instanceof",
    "interface",    "native", "new",        "package",  "private", "protected",
    "public",       "return", "static",     "strictfp", "super",   "switch",
    "synchronized", "this",   "throw",      "throws",   "trait",   "try",
    "while",
};
static const char *const groovy_ty[] = {
    "boolean", "byte",   "char", "double", "float", "int",
    "long",    "short",  "void", "def",    "null",  "true",
    "false",   "String", "List", "Map",    "Set",
};
static const char *const powershell_kw[] = {
    "begin",   "break",    "catch",        "class",   "continue", "data",
    "define",  "do",       "dynamicparam", "else",    "elseif",   "end",
    "enum",    "exit",     "filter",       "finally", "for",      "foreach",
    "from",    "function", "hidden",       "if",      "in",       "param",
    "process", "return",   "static",       "switch",  "throw",    "trap",
    "try",     "until",    "using",        "while",
};
static const char *const awk_kw[] = {
    "BEGIN",    "END",   "break",    "continue", "delete", "do", "else",
    "exit",     "for",   "function", "getline",  "if",     "in", "next",
    "nextfile", "print", "printf",   "return",   "while",
};
static const char *const fish_kw[] = {
    "and",  "begin",  "break", "builtin",  "case",   "command", "continue",
    "else", "end",    "for",   "function", "if",     "in",      "not",
    "or",   "return", "set",   "status",   "switch", "test",    "while",
};

#define WS(arr)                                                                \
    (struct wordset)                                                           \
    {                                                                          \
        arr, (int)(sizeof(arr) / sizeof(arr[0]))                               \
    }

/* ---- Perl / PHP (# or // comments, $vars, strings) ---- */

static int lex_perish(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if ((c == '#' || (c == '/' && i + 1 < len && d[i + 1] == '/')) &&
            h->state == HL_NORMAL) {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '$' || (c == '@' && i + 1 < len && is_word(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Haskell / OCaml (-- line comments, {- -} block) ---- */

static int lex_haskell(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '-' && d[i + 1] == '}') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '-' && i + 1 < len && d[i + 1] == '-') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '{' && i + 1 < len && d[i + 1] == '-') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '-' && d[i + 1] == '}') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '\'') {
            i++;
            if (i < len && d[i] == '\\')
                i++;
            if (i < len)
                i++;
            if (i < len && d[i] == '\'')
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '\''))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Clojure (; comments, keywords as :word) ---- */

static int lex_clojure(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == ';') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == ':') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_CONSTANT);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '/'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c) || c == '-' || c == '+' || c == '*' || c == '!' ||
                   c == '?') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-' || d[i] == '_' ||
                               d[i] == '!' || d[i] == '?' || d[i] == '*'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- R (# comments, <- assignment) ---- */

static int lex_r(struct mat_hl *h, const unsigned char *d, size_t len,
                 struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '<' && i + 1 < len && d[i + 1] == '-') {
            i += 2;
            n = emit(out, cap, n, s, 2, MT_OPERATOR);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c) || c == '.') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '.'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Dockerfile ---- */

static int lex_dockerfile(struct mat_hl *h, const unsigned char *d, size_t len,
                          struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '$') {
            i++;
            if (i < len && d[i] == '{') {
                while (i < len && d[i] != '}')
                    i++;
                if (i < len)
                    i++;
            } else {
                while (i < len && is_alnum(d[i]))
                    i++;
            }
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ci_match("FROM", (const char *)d + s, wl) ||
                ci_match("RUN", (const char *)d + s, wl) ||
                ci_match("CMD", (const char *)d + s, wl) ||
                ci_match("COPY", (const char *)d + s, wl) ||
                ci_match("ADD", (const char *)d + s, wl) ||
                ci_match("ENV", (const char *)d + s, wl) ||
                ci_match("EXPOSE", (const char *)d + s, wl) ||
                ci_match("ENTRYPOINT", (const char *)d + s, wl) ||
                ci_match("WORKDIR", (const char *)d + s, wl) ||
                ci_match("ARG", (const char *)d + s, wl) ||
                ci_match("LABEL", (const char *)d + s, wl) ||
                ci_match("VOLUME", (const char *)d + s, wl) ||
                ci_match("USER", (const char *)d + s, wl) ||
                ci_match("HEALTHCHECK", (const char *)d + s, wl) ||
                ci_match("SHELL", (const char *)d + s, wl) ||
                ci_match("STOPSIGNAL", (const char *)d + s, wl) ||
                ci_match("ONBUILD", (const char *)d + s, wl) ||
                ci_match("MAINTAINER", (const char *)d + s, wl) ||
                ci_match("AS", (const char *)d + s, wl))
                t = MT_KEYWORD;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- INI / .conf ---- */

static int lex_ini(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len > 0 && (d[0] == ';' || d[0] == '#')) {
        n = emit(out, cap, n, 0, len, MT_COMMENT);
        return n;
    }
    if (len > 0 && d[0] == '[') {
        n = emit(out, cap, n, 0, len, MT_KEYWORD);
        return n;
    }
    while (i < len) {
        if (d[i] == '=') {
            n = emit(out, cap, n, 0, i, MT_TYPE);
            n = emit(out, cap, n, i, 1, MT_OPERATOR);
            if (i + 1 < len)
                n = emit(out, cap, n, i + 1, len - i - 1, MT_STRING);
            return n;
        }
        i++;
    }
    n = emit(out, cap, n, 0, len, MT_TEXT);
    return n;
}

/* ---- LaTeX ---- */

static int lex_latex(struct mat_hl *h, const unsigned char *d, size_t len,
                     struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '%') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '\\') {
            i++;
            while (i < len && is_word(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (c == '{' || c == '}') {
            i++;
            n = emit(out, cap, n, s, 1, MT_PUNCT);
        } else if (c == '$') {
            i++;
            while (i < len && d[i] != '$')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '[' || c == ']') {
            i++;
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
        } else {
            i++;
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
    struct wordset kw = {NULL, 0}, ty = {NULL, 0};
    if (strcmp(syntax, "JSON") == 0) {
        lex = lex_json;
    } else if (strcmp(syntax, "C") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    } else if (strcmp(syntax, "C++") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    } else if (strcmp(syntax, "Java") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    } else if (strcmp(syntax, "JavaScript") == 0 ||
               strcmp(syntax, "TypeScript") == 0) {
        lex = lex_cfamily;
        kw = WS(js_kw);
        ty = WS(js_ty);
    } else if (strcmp(syntax, "Go") == 0) {
        lex = lex_cfamily;
        kw = WS(go_kw);
        ty = WS(go_ty);
    } else if (strcmp(syntax, "Rust") == 0) {
        lex = lex_cfamily;
        kw = WS(rs_kw);
        ty = WS(rs_ty);
    } else if (strcmp(syntax, "Python") == 0) {
        lex = lex_python;
        kw = WS(py_kw);
        ty = WS(py_ty);
    } else if (strcmp(syntax, "Bash") == 0 || strcmp(syntax, "Zsh") == 0) {
        lex = lex_shell;
        kw = WS(sh_kw);
    } else if (strcmp(syntax, "Fortran") == 0) {
        lex = lex_fortran;
        kw = WS(fortran_kw);
        ty = WS(fortran_ty);
    } else if (strcmp(syntax, "Markdown") == 0) {
        lex = lex_markdown;
    } else if (strcmp(syntax, "YAML") == 0) {
        lex = lex_yaml;
    } else if (strcmp(syntax, "TOML") == 0) {
        lex = lex_toml;
    } else if (strcmp(syntax, "HTML") == 0) {
        lex = lex_html;
    } else if (strcmp(syntax, "CSS") == 0 || strcmp(syntax, "SCSS") == 0) {
        lex = lex_css;
    } else if (strcmp(syntax, "SQL") == 0) {
        lex = lex_sql;
        kw = WS(sql_kw);
        ty = WS(sql_ty);
    } else if (strcmp(syntax, "Ruby") == 0) {
        lex = lex_ruby;
        kw = WS(ruby_kw);
        ty = WS(ruby_ty);
    } else if (strcmp(syntax, "Lua") == 0) {
        lex = lex_lua;
        kw = WS(lua_kw);
        ty = WS(lua_ty);
    } else if (strcmp(syntax, "Makefile") == 0) {
        lex = lex_makefile;
    } else if (strcmp(syntax, "Diff") == 0) {
        lex = lex_diff;
    } else if (strcmp(syntax, "C#") == 0) {
        lex = lex_cfamily;
        kw = WS(cs_kw);
        ty = WS(cs_ty);
    } else if (strcmp(syntax, "Kotlin") == 0) {
        lex = lex_cfamily;
        kw = WS(kt_kw);
        ty = WS(kt_ty);
    } else if (strcmp(syntax, "Scala") == 0) {
        lex = lex_cfamily;
        kw = WS(scala_kw);
        ty = WS(scala_ty);
    } else if (strcmp(syntax, "Swift") == 0) {
        lex = lex_cfamily;
        kw = WS(swift_kw);
        ty = WS(swift_ty);
    } else if (strcmp(syntax, "Dart") == 0) {
        lex = lex_cfamily;
        kw = WS(dart_kw);
        ty = WS(dart_ty);
    } else if (strcmp(syntax, "Zig") == 0) {
        lex = lex_cfamily;
        kw = WS(zig_kw);
        ty = WS(zig_ty);
    } else if (strcmp(syntax, "Nim") == 0) {
        lex = lex_cfamily;
        kw = WS(nim_kw);
        ty = WS(nim_ty);
    } else if (strcmp(syntax, "Groovy") == 0) {
        lex = lex_cfamily;
        kw = WS(groovy_kw);
        ty = WS(groovy_ty);
    } else if (strcmp(syntax, "Perl") == 0) {
        lex = lex_perish;
        kw = WS(perl_kw);
    } else if (strcmp(syntax, "PHP") == 0) {
        lex = lex_perish;
        kw = WS(php_kw);
        ty = WS(php_ty);
    } else if (strcmp(syntax, "Haskell") == 0) {
        lex = lex_haskell;
        kw = WS(haskell_kw);
        ty = WS(haskell_ty);
    } else if (strcmp(syntax, "OCaml") == 0) {
        lex = lex_haskell;
        kw = WS(ocaml_kw);
        ty = WS(ocaml_ty);
    } else if (strcmp(syntax, "Elixir") == 0) {
        lex = lex_python;
        kw = WS(elixir_kw);
        ty = WS(elixir_ty);
    } else if (strcmp(syntax, "Erlang") == 0) {
        lex = lex_python;
        kw = WS(erlang_kw);
        ty = WS(erlang_ty);
    } else if (strcmp(syntax, "R") == 0) {
        lex = lex_r;
        kw = WS(r_kw);
        ty = WS(r_ty);
    } else if (strcmp(syntax, "Clojure") == 0) {
        lex = lex_clojure;
        kw = WS(clojure_kw);
        ty = WS(clojure_ty);
    } else if (strcmp(syntax, "Julia") == 0) {
        lex = lex_python;
        kw = WS(julia_kw);
        ty = WS(julia_ty);
    } else if (strcmp(syntax, "Dockerfile") == 0) {
        lex = lex_dockerfile;
    } else if (strcmp(syntax, "INI") == 0) {
        lex = lex_ini;
    } else if (strcmp(syntax, "LaTeX") == 0) {
        lex = lex_latex;
    } else if (strcmp(syntax, "PowerShell") == 0) {
        lex = lex_shell;
        kw = WS(powershell_kw);
    } else if (strcmp(syntax, "AWK") == 0) {
        lex = lex_shell;
        kw = WS(awk_kw);
    } else if (strcmp(syntax, "Fish") == 0) {
        lex = lex_shell;
        kw = WS(fish_kw);
    } else if (strcmp(syntax, "Objective-C") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    }
    if (lex == NULL)
        return NULL;
    struct mat_hl *h = calloc(1, sizeof *h);
    if (h != NULL) {
        h->lex = lex;
        h->keywords = kw;
        h->types = ty;
    }
    return h;
}

void mat_hl_close(struct mat_hl *h)
{
    free(h);
}

void mat_hl_list_languages(void)
{
    static const char *const langs[] = {
        "AWK",        "Bash",     "C",        "C#",         "C++",
        "Clojure",    "CSS",      "Dart",     "Diff",       "Dockerfile",
        "Elixir",     "Erlang",   "Fish",     "Fortran",    "Go",
        "Groovy",     "Haskell",  "HTML",     "INI",        "Java",
        "JavaScript", "JSON",     "Julia",    "Kotlin",     "LaTeX",
        "Lua",        "Makefile", "Markdown", "Nim",        "Objective-C",
        "OCaml",      "Perl",     "PHP",      "PowerShell", "Python",
        "R",          "Ruby",     "Rust",     "Scala",      "SCSS",
        "SQL",        "Swift",    "TOML",     "TypeScript", "YAML",
        "Zig",        "Zsh",
    };
    for (size_t i = 0; i < sizeof langs / sizeof langs[0]; i++)
        printf("%s\n", langs[i]);
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
