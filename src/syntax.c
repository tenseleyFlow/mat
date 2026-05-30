#include "syntax.h"
#include "config.h"

#include <fnmatch.h>
#include <string.h>

struct kv {
    const char *key;
    const char *val;
};

/* Whole basename → syntax (checked before extensions). */
static const struct kv by_name[] = {
    {"Makefile", "Makefile"},     {"makefile", "Makefile"},
    {"GNUmakefile", "Makefile"},  {"Dockerfile", "Dockerfile"},
    {"CMakeLists.txt", "CMake"},  {".gitignore", "Git Ignore"},
    {".gitconfig", "Git Config"}, {".bashrc", "Bash"},
    {".bash_profile", "Bash"},    {".zshrc", "Zsh"},
    {".vimrc", "VimL"},           {"Cargo.toml", "TOML"},
    {"go.mod", "Go Module"},
};

/* File extension → syntax. */
static const struct kv by_ext[] = {
    {"c", "C"},
    {"h", "C"},
    {"cc", "C++"},
    {"cpp", "C++"},
    {"cxx", "C++"},
    {"hpp", "C++"},
    {"hh", "C++"},
    {"py", "Python"},
    {"js", "JavaScript"},
    {"mjs", "JavaScript"},
    {"ts", "TypeScript"},
    {"tsx", "TypeScript"},
    {"jsx", "JavaScript"},
    {"json", "JSON"},
    {"md", "Markdown"},
    {"markdown", "Markdown"},
    {"sh", "Bash"},
    {"bash", "Bash"},
    {"zsh", "Zsh"},
    {"rs", "Rust"},
    {"go", "Go"},
    {"rb", "Ruby"},
    {"java", "Java"},
    {"html", "HTML"},
    {"htm", "HTML"},
    {"css", "CSS"},
    {"scss", "SCSS"},
    {"xml", "XML"},
    {"yaml", "YAML"},
    {"yml", "YAML"},
    {"toml", "TOML"},
    {"ini", "INI"},
    {"conf", "INI"},
    {"sql", "SQL"},
    {"lua", "Lua"},
    {"pl", "Perl"},
    {"pm", "Perl"},
    {"php", "PHP"},
    {"swift", "Swift"},
    {"kt", "Kotlin"},
    {"scala", "Scala"},
    {"hs", "Haskell"},
    {"ml", "OCaml"},
    {"ex", "Elixir"},
    {"exs", "Elixir"},
    {"clj", "Clojure"},
    {"vim", "VimL"},
    {"tex", "LaTeX"},
    {"diff", "Diff"},
    {"patch", "Diff"},
    {"csv", "CSV"},
    {"txt", "Plain Text"},
};

/* Shebang interpreter basename → syntax. */
static const struct kv by_interp[] = {
    {"sh", "Bash"},       {"bash", "Bash"},       {"zsh", "Zsh"},
    {"python", "Python"}, {"python3", "Python"},  {"perl", "Perl"},
    {"ruby", "Ruby"},     {"node", "JavaScript"}, {"lua", "Lua"},
    {"awk", "AWK"},       {"php", "PHP"},
};

static const char *lookup(const struct kv *t, size_t n, const char *key)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(t[i].key, key) == 0)
            return t[i].val;
    return NULL;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Strip a trailing --ignored-suffix (e.g. file.c.bak -> file.c), once. */
static size_t strip_ignored(const struct config *cfg, const char *base,
                            size_t len)
{
    for (int i = 0; i < cfg->nsuffix; i++) {
        size_t sl = strlen(cfg->ignored_suffix[i]);
        if (sl > 0 && sl < len &&
            memcmp(base + len - sl, cfg->ignored_suffix[i], sl) == 0)
            return len - sl;
    }
    return len;
}

/* Resolve a syntax name from the shebang of the first line, or NULL. */
static const char *from_shebang(const unsigned char *first, size_t n)
{
    if (n < 3 || first[0] != '#' || first[1] != '!')
        return NULL;
    /* take the last path component of the first whitespace-delimited token,
     * then if it is "env", the following token. */
    const char *s = (const char *)first + 2;
    const char *end = (const char *)first + n;
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    const char *tok = s;
    while (s < end && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
        s++;
    /* basename of tok..s */
    const char *tb = tok;
    for (const char *p = tok; p < s; p++)
        if (*p == '/')
            tb = p + 1;
    size_t tlen = (size_t)(s - tb);
    char word[64];
    if (tlen >= sizeof word)
        tlen = sizeof word - 1;
    memcpy(word, tb, tlen);
    word[tlen] = '\0';

    if (strcmp(word, "env") == 0) {
        while (s < end && (*s == ' ' || *s == '\t'))
            s++;
        const char *a = s;
        while (s < end && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
            s++;
        size_t alen = (size_t)(s - a);
        if (alen >= sizeof word)
            alen = sizeof word - 1;
        memcpy(word, a, alen);
        word[alen] = '\0';
    }
    return lookup(by_interp, sizeof by_interp / sizeof by_interp[0], word);
}

const char *mat_syntax_detect(const struct config *cfg, const char *name,
                              const unsigned char *first, size_t first_len)
{
    if (cfg->language && cfg->language[0])
        return cfg->language;

    const char *base = base_name(name);
    size_t blen = strip_ignored(cfg, base, strlen(base));

    /* --map-syntax globs, matched against the (suffix-stripped) basename. */
    char bbuf[1024];
    size_t bcopy = blen < sizeof bbuf ? blen : sizeof bbuf - 1;
    memcpy(bbuf, base, bcopy);
    bbuf[bcopy] = '\0';
    for (int i = 0; i < cfg->nmaps; i++)
        if (fnmatch(cfg->map_glob[i], bbuf, 0) == 0)
            return cfg->map_syntax[i];

    /* Whole-name table (Makefile, Dockerfile, dotfiles, ...). */
    const char *hit = lookup(by_name, sizeof by_name / sizeof by_name[0], bbuf);
    if (hit)
        return hit;

    /* Extension. */
    const char *dot = NULL;
    for (size_t i = 0; i < bcopy; i++)
        if (bbuf[i] == '.')
            dot = bbuf + i;
    if (dot && dot[1])
        if ((hit = lookup(by_ext, sizeof by_ext / sizeof by_ext[0], dot + 1)))
            return hit;

    /* First-line shebang. */
    if ((hit = from_shebang(first, first_len)))
        return hit;

    if (cfg->fallback_syntax && cfg->fallback_syntax[0])
        return cfg->fallback_syntax;
    return "plain";
}
