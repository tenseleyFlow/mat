#include "syntax.h"
#include "config.h"

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>

struct kv {
    const char *key;
    const char *val;
};

/* Whole basename → syntax (checked before extensions). */
static const struct kv by_name[] = {
    {".bash_profile", "Bash"},
    {".bashrc", "Bash"},
    {".editorconfig", "INI"},
    {".env", "INI"},
    {".fishrc", "Fish"},
    {".gitattributes", "Git Attributes"},
    {".gitconfig", "Git Config"},
    {".gitignore", "Git Ignore"},
    {".gitmodules", "Git Config"},
    {".htaccess", "Apache Conf"},
    {".profile", "Bash"},
    {".vimrc", "VimL"},
    {".zshrc", "Zsh"},
    {"BUILD", "Python"},
    {"CMakeLists.txt", "CMake"},
    {"COMMIT_EDITMSG", "Git Commit"},
    {"Cargo.toml", "TOML"},
    {"Containerfile", "Dockerfile"},
    {"Dockerfile", "Dockerfile"},
    {"GNUmakefile", "Makefile"},
    {"Gemfile", "Ruby"},
    {"Jenkinsfile", "Groovy"},
    {"MERGE_MSG", "Git Commit"},
    {"Makefile", "Makefile"},
    {"Pipfile", "TOML"},
    {"Puppetfile", "Puppet"},
    {"Rakefile", "Ruby"},
    {"TAG_EDITMSG", "Git Commit"},
    {"Vagrantfile", "Ruby"},
    {"WORKSPACE", "Python"},
    {"authorized_keys", "Authorized Keys"},
    {"build.ninja", "Ninja"},
    {"config.fish", "Fish"},
    {"default.nix", "Nix"},
    {"flake.nix", "Nix"},
    {"git-rebase-todo", "Git Rebase Todo"},
    {"go.mod", "Go Module"},
    {"httpd.conf", "Apache Conf"},
    {"known_hosts", "Known Hosts"},
    {"makefile", "Makefile"},
    {"nginx.conf", "nginx"},
    {"requirements.txt", "Requirements.txt"},
    {"shell.nix", "Nix"},
    {"ssh_config", "SSH Config"},
    {"sshd_config", "SSHD Config"},
    {"todo.txt", "Todo.txt"},
};

/* File extension → syntax. */
static const struct kv by_ext[] = {
    {"R", "R"},
    {"Rd", "Rd"},
    {"S", "Assembly"},
    {"ada", "Ada"},
    {"adb", "Ada"},
    {"adoc", "AsciiDoc"},
    {"ads", "Ada"},
    {"applescript", "AppleScript"},
    {"as", "ActionScript"},
    {"asciidoc", "AsciiDoc"},
    {"asm", "Assembly"},
    {"awk", "AWK"},
    {"bash", "Bash"},
    {"bat", "Batch File"},
    {"bib", "BibTeX"},
    {"c", "C"},
    {"cabal", "Cabal"},
    {"cc", "C++"},
    {"cfc", "CFML"},
    {"cfg", "INI"},
    {"cfm", "CFML"},
    {"clj", "Clojure"},
    {"cljc", "Clojure"},
    {"cljs", "Clojure"},
    {"cls", "LaTeX"},
    {"cmake", "CMake"},
    {"cmd", "Batch File"},
    {"coffee", "CoffeeScript"},
    {"comp", "GLSL"},
    {"conf", "INI"},
    {"cpp", "C++"},
    {"cr", "Crystal"},
    {"cs", "C#"},
    {"css", "CSS"},
    {"csv", "CSV"},
    {"cxx", "C++"},
    {"d", "D"},
    {"dart", "Dart"},
    {"di", "D"},
    {"diff", "Diff"},
    {"dot", "Graphviz"},
    {"dpr", "Pascal"},
    {"el", "Lisp"},
    {"elm", "Elm"},
    {"env", "DotENV"},
    {"erl", "Erlang"},
    {"ex", "Elixir"},
    {"exs", "Elixir"},
    {"f", "Fortran"},
    {"f03", "Fortran"},
    {"f08", "Fortran"},
    {"f18", "Fortran"},
    {"f90", "Fortran"},
    {"f95", "Fortran"},
    {"fish", "Fish"},
    {"for", "Fortran"},
    {"fpp", "Fortran"},
    {"frag", "GLSL"},
    {"fs", "F#"},
    {"fsi", "F#"},
    {"fsx", "F#"},
    {"fun", "SML"},
    {"geom", "GLSL"},
    {"glsl", "GLSL"},
    {"gnuplot", "gnuplot"},
    {"go", "Go"},
    {"gp", "gnuplot"},
    {"gql", "GraphQL"},
    {"graphql", "GraphQL"},
    {"groff", "Groff"},
    {"groovy", "Groovy"},
    {"gv", "Graphviz"},
    {"gvy", "Groovy"},
    {"h", "C"},
    {"hcl", "Terraform"},
    {"hh", "C++"},
    {"hpp", "C++"},
    {"hrl", "Erlang"},
    {"hs", "Haskell"},
    {"htm", "HTML"},
    {"html", "HTML"},
    {"http", "HTTP"},
    {"ini", "INI"},
    {"j2", "Jinja2"},
    {"java", "Java"},
    {"jinja", "Jinja2"},
    {"jinja2", "Jinja2"},
    {"jl", "Julia"},
    {"jq", "JQ"},
    {"js", "JavaScript"},
    {"json", "JSON"},
    {"jsonnet", "jsonnet"},
    {"jsx", "JavaScript"},
    {"kt", "Kotlin"},
    {"kts", "Kotlin"},
    {"lean", "Lean"},
    {"less", "Less"},
    {"lhs", "Haskell"},
    {"libsonnet", "jsonnet"},
    {"lisp", "Lisp"},
    {"ll", "LLVM"},
    {"lpr", "Pascal"},
    {"ls", "LiveScript"},
    {"lsp", "Lisp"},
    {"lua", "Lua"},
    {"m", "Objective-C"},
    {"man", "Manpage"},
    {"markdown", "Markdown"},
    {"mat", "MATLAB"},
    {"md", "Markdown"},
    {"mjs", "JavaScript"},
    {"ml", "OCaml"},
    {"mli", "OCaml"},
    {"mm", "Objective-C"},
    {"nim", "Nim"},
    {"ninja", "Ninja"},
    {"nix", "Nix"},
    {"nsh", "NSIS"},
    {"nsi", "NSIS"},
    {"org", "orgmode"},
    {"pas", "Pascal"},
    {"patch", "Diff"},
    {"php", "PHP"},
    {"pl", "Perl"},
    {"plist", "XML"},
    {"plt", "gnuplot"},
    {"pm", "Perl"},
    {"pp", "Pascal"},
    {"properties", "Java Properties"},
    {"proto", "Protobuf"},
    {"ps1", "PowerShell"},
    {"psm1", "PowerShell"},
    {"purs", "PureScript"},
    {"py", "Python"},
    {"qml", "QML"},
    {"r", "R"},
    {"rb", "Ruby"},
    {"rd", "Rd"},
    {"rego", "Rego"},
    {"rest", "reStructuredText"},
    {"rkt", "Racket"},
    {"robot", "Robot Framework"},
    {"roff", "Groff"},
    {"rs", "Rust"},
    {"rst", "reStructuredText"},
    {"s", "Assembly"},
    {"sass", "Sass"},
    {"sc", "Scala"},
    {"scala", "Scala"},
    {"scm", "Lisp"},
    {"scpt", "AppleScript"},
    {"scss", "SCSS"},
    {"sh", "Bash"},
    {"sig", "SML"},
    {"sls", "Salt State"},
    {"sml", "SML"},
    {"sol", "Solidity"},
    {"sql", "SQL"},
    {"sty", "LaTeX"},
    {"styl", "Stylus"},
    {"sv", "SystemVerilog"},
    {"svelte", "Svelte"},
    {"svg", "SVG"},
    {"svh", "SystemVerilog"},
    {"swift", "Swift"},
    {"tcl", "Tcl"},
    {"tex", "LaTeX"},
    {"textile", "Textile"},
    {"tf", "Terraform"},
    {"tfvars", "Terraform"},
    {"tk", "Tcl"},
    {"toml", "TOML"},
    {"troff", "Groff"},
    {"ts", "TypeScript"},
    {"tsx", "TypeScript"},
    {"txt", "Plain Text"},
    {"v", "Verilog"},
    {"varlink", "varlink"},
    {"vert", "GLSL"},
    {"vh", "Verilog"},
    {"vim", "VimL"},
    {"vue", "Vue"},
    {"vy", "Vyper"},
    {"wgsl", "WGSL"},
    {"wiki", "MediaWiki"},
    {"xml", "XML"},
    {"xsl", "XML"},
    {"xslt", "XML"},
    {"yaml", "YAML"},
    {"yml", "YAML"},
    {"zig", "Zig"},
    {"zsh", "Zsh"},
};

/* Shebang interpreter basename → syntax. */
static const struct kv by_interp[] = {
    {"Rscript", "R"},      {"awk", "AWK"},         {"bash", "Bash"},
    {"dart", "Dart"},      {"elixir", "Elixir"},   {"fish", "Fish"},
    {"gawk", "AWK"},       {"julia", "Julia"},     {"lua", "Lua"},
    {"mawk", "AWK"},       {"node", "JavaScript"}, {"perl", "Perl"},
    {"php", "PHP"},        {"pwsh", "PowerShell"}, {"python", "Python"},
    {"python3", "Python"}, {"ruby", "Ruby"},       {"sh", "Bash"},
    {"zsh", "Zsh"},
};

static int kv_cmp(const void *key, const void *elem)
{
    return strcmp((const char *)key, ((const struct kv *)elem)->key);
}

static const char *lookup(const struct kv *t, size_t n, const char *key)
{
    const struct kv *hit = bsearch(key, t, n, sizeof *t, kv_cmp);
    return hit ? hit->val : NULL;
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
