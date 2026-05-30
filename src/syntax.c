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
    {"Makefile", "Makefile"},
    {"makefile", "Makefile"},
    {"GNUmakefile", "Makefile"},
    {"Dockerfile", "Dockerfile"},
    {"CMakeLists.txt", "CMake"},
    {".gitignore", "Git Ignore"},
    {".gitconfig", "Git Config"},
    {".bashrc", "Bash"},
    {".bash_profile", "Bash"},
    {".zshrc", "Zsh"},
    {".vimrc", "VimL"},
    {"Cargo.toml", "TOML"},
    {"go.mod", "Go Module"},
    {"Vagrantfile", "Ruby"},
    {"Rakefile", "Ruby"},
    {"Gemfile", "Ruby"},
    {"Containerfile", "Dockerfile"},
    {".editorconfig", "INI"},
    {".env", "INI"},
    {".profile", "Bash"},
    {".fishrc", "Fish"},
    {"config.fish", "Fish"},
    {"nginx.conf", "nginx"},
    {"httpd.conf", "Apache Conf"},
    {".htaccess", "Apache Conf"},
    {"requirements.txt", "Requirements.txt"},
    {"Pipfile", "TOML"},
    {"flake.nix", "Nix"},
    {"default.nix", "Nix"},
    {"shell.nix", "Nix"},
    {"Puppetfile", "Puppet"},
    {"Jenkinsfile", "Groovy"},
    {"BUILD", "Python"},
    {"WORKSPACE", "Python"},
    {"COMMIT_EDITMSG", "Git Commit"},
    {"MERGE_MSG", "Git Commit"},
    {"TAG_EDITMSG", "Git Commit"},
    {"git-rebase-todo", "Git Rebase Todo"},
    {".gitattributes", "Git Attributes"},
    {".gitmodules", "Git Config"},
    {"ssh_config", "SSH Config"},
    {"sshd_config", "SSHD Config"},
    {"authorized_keys", "Authorized Keys"},
    {"known_hosts", "Known Hosts"},
    {"todo.txt", "Todo.txt"},
    {"build.ninja", "Ninja"},
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
    {"f", "Fortran"},
    {"f90", "Fortran"},
    {"f95", "Fortran"},
    {"f03", "Fortran"},
    {"f08", "Fortran"},
    {"f18", "Fortran"},
    {"for", "Fortran"},
    {"fpp", "Fortran"},
    {"cs", "C#"},
    {"kts", "Kotlin"},
    {"dart", "Dart"},
    {"zig", "Zig"},
    {"nim", "Nim"},
    {"groovy", "Groovy"},
    {"gvy", "Groovy"},
    {"lhs", "Haskell"},
    {"mli", "OCaml"},
    {"erl", "Erlang"},
    {"hrl", "Erlang"},
    {"r", "R"},
    {"R", "R"},
    {"cljs", "Clojure"},
    {"cljc", "Clojure"},
    {"jl", "Julia"},
    {"cfg", "INI"},
    {"sty", "LaTeX"},
    {"cls", "LaTeX"},
    {"ps1", "PowerShell"},
    {"psm1", "PowerShell"},
    {"awk", "AWK"},
    {"fish", "Fish"},
    {"m", "Objective-C"},
    {"mm", "Objective-C"},
    {"sc", "Scala"},
    {"d", "D"},
    {"di", "D"},
    {"fs", "F#"},
    {"fsi", "F#"},
    {"fsx", "F#"},
    {"glsl", "GLSL"},
    {"vert", "GLSL"},
    {"frag", "GLSL"},
    {"geom", "GLSL"},
    {"comp", "GLSL"},
    {"coffee", "CoffeeScript"},
    {"cr", "Crystal"},
    {"elm", "Elm"},
    {"sol", "Solidity"},
    {"adb", "Ada"},
    {"ads", "Ada"},
    {"ada", "Ada"},
    {"pas", "Pascal"},
    {"pp", "Pascal"},
    {"lpr", "Pascal"},
    {"dpr", "Pascal"},
    {"mat", "MATLAB"},
    {"proto", "Protobuf"},
    {"tf", "Terraform"},
    {"tfvars", "Terraform"},
    {"hcl", "Terraform"},
    {"nix", "Nix"},
    {"tcl", "Tcl"},
    {"tk", "Tcl"},
    {"lisp", "Lisp"},
    {"lsp", "Lisp"},
    {"el", "Lisp"},
    {"scm", "Lisp"},
    {"rkt", "Racket"},
    {"asm", "Assembly"},
    {"s", "Assembly"},
    {"S", "Assembly"},
    {"bat", "Batch File"},
    {"cmd", "Batch File"},
    {"gv", "Graphviz"},
    {"dot", "Graphviz"},
    {"purs", "PureScript"},
    {"sml", "SML"},
    {"sig", "SML"},
    {"fun", "SML"},
    {"xsl", "XML"},
    {"xslt", "XML"},
    {"svg", "SVG"},
    {"plist", "XML"},
    {"graphql", "GraphQL"},
    {"gql", "GraphQL"},
    {"cmake", "CMake"},
    {"sass", "Sass"},
    {"less", "Less"},
    {"styl", "Stylus"},
    {"jsonnet", "jsonnet"},
    {"libsonnet", "jsonnet"},
    {"qml", "QML"},
    {"ll", "LLVM"},
    {"as", "ActionScript"},
    {"applescript", "AppleScript"},
    {"scpt", "AppleScript"},
    {"wgsl", "WGSL"},
    {"rego", "Rego"},
    {"vy", "Vyper"},
    {"properties", "Java Properties"},
    {"env", "DotENV"},
    {"lean", "Lean"},
    {"svelte", "Svelte"},
    {"vue", "Vue"},
    {"j2", "Jinja2"},
    {"jinja", "Jinja2"},
    {"jinja2", "Jinja2"},
    {"adoc", "AsciiDoc"},
    {"asciidoc", "AsciiDoc"},
    {"rst", "reStructuredText"},
    {"rest", "reStructuredText"},
    {"wiki", "MediaWiki"},
    {"org", "orgmode"},
    {"groff", "Groff"},
    {"troff", "Groff"},
    {"roff", "Groff"},
    {"man", "Manpage"},
    {"bib", "BibTeX"},
    {"gp", "gnuplot"},
    {"gnuplot", "gnuplot"},
    {"plt", "gnuplot"},
    {"v", "Verilog"},
    {"sv", "SystemVerilog"},
    {"svh", "SystemVerilog"},
    {"vh", "Verilog"},
    {"ninja", "Ninja"},
    {"nsi", "NSIS"},
    {"nsh", "NSIS"},
    {"jq", "JQ"},
    {"ls", "LiveScript"},
    {"cabal", "Cabal"},
    {"cfm", "CFML"},
    {"cfc", "CFML"},
    {"robot", "Robot Framework"},
    {"sls", "Salt State"},
    {"rd", "Rd"},
    {"Rd", "Rd"},
    {"textile", "Textile"},
    {"varlink", "varlink"},
    {"http", "HTTP"},
};

/* Shebang interpreter basename → syntax. */
static const struct kv by_interp[] = {
    {"sh", "Bash"},       {"bash", "Bash"},       {"zsh", "Zsh"},
    {"python", "Python"}, {"python3", "Python"},  {"perl", "Perl"},
    {"ruby", "Ruby"},     {"node", "JavaScript"}, {"lua", "Lua"},
    {"awk", "AWK"},       {"php", "PHP"},         {"fish", "Fish"},
    {"Rscript", "R"},     {"elixir", "Elixir"},   {"julia", "Julia"},
    {"dart", "Dart"},     {"pwsh", "PowerShell"}, {"gawk", "AWK"},
    {"mawk", "AWK"},
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
