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

#define WS(arr)                                                                \
    (struct wordset)                                                           \
    {                                                                          \
        arr, (int)(sizeof(arr) / sizeof(arr[0]))                               \
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
