#include "unity.h"
#include "highlight.h"

void setUp(void)
{
}
void tearDown(void)
{
}

static void test_all_tables_sorted(void)
{
    const char *bad = mat_hl_validate_tables();
    if (bad != NULL)
        TEST_FAIL_MESSAGE(bad);
}

static void test_every_lang_opens(void)
{
    const char *names[] = {
        "C",          "Python",     "Bash",
        "JavaScript", "Ruby",       "Go",
        "Rust",       "Haskell",    "Markdown",
        "YAML",       "JSON",       "HTML",
        "CSS",        "SQL",        "Lua",
        "Makefile",   "Diff",       "LaTeX",
        "Fortran",    "TOML",       "Assembly",
        "Pascal",     "MATLAB",     "VimL",
        "Dockerfile", "INI",        "Clojure",
        "R",          "Kotlin",     "Scala",
        "Swift",      "Dart",       "Zig",
        "Nim",        "Groovy",     "Perl",
        "PHP",        "OCaml",      "Elixir",
        "Erlang",     "Julia",      "Strace",
        "Todo.txt",   "VimHelp",    "JQ",
        "Crontab",    "Git Commit", "Git Rebase Todo",
        "SSH Config", "passwd",     "Literate Haskell",
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        struct mat_hl *h = mat_hl_open(names[i]);
        if (h == NULL) {
            char msg[128];
            snprintf(msg, sizeof msg, "mat_hl_open(\"%s\") returned NULL",
                     names[i]);
            TEST_FAIL_MESSAGE(msg);
        }
        mat_hl_close(h);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_tables_sorted);
    RUN_TEST(test_every_lang_opens);
    return UNITY_END();
}
