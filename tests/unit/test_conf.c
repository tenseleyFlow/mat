#include "unity.h"
#include "conf.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static void check(const char *in, bool comments, const char *const *want,
                  int wantn)
{
    char **t;
    int n = mat_tokenize(in, comments, &t);
    TEST_ASSERT_EQUAL_INT(wantn, n);
    for (int i = 0; i < n && i < wantn; i++)
        TEST_ASSERT_EQUAL_STRING(want[i], t[i]);
    mat_tokens_free(t, n);
}

static void test_basic(void)
{
    const char *w[] = {"--style=full", "-n"};
    check("--style=full -n", false, w, 2);
}

static void test_whitespace(void)
{
    const char *w[] = {"a", "b", "c"};
    check("  a\t b \n c  ", false, w, 3);
}

static void test_quotes(void)
{
    /* quoted run keeps its spaces and joins the adjacent token */
    const char *w[] = {"--style=a b", "x"};
    check("--style=\"a b\" x", false, w, 2);
    const char *w2[] = {"one two"};
    check("'one two'", false, w2, 1);
}

static void test_comments(void)
{
    const char *w[] = {"--tabs=2"};
    check("# a comment\n--tabs=2 # trailing\n", true, w, 1);
    /* '#' is literal when comments are disabled (e.g. inside $MAT_OPTS) */
    const char *w2[] = {"a#b"};
    check("a#b", false, w2, 1);
}

static void test_empty(void)
{
    char **t;
    int n = mat_tokenize("   \n\t  ", false, &t);
    TEST_ASSERT_EQUAL_INT(0, n);
    mat_tokens_free(t, n);
}

static void test_empty_double_quote(void)
{
    const char *w[] = {"", "x"};
    check("\"\" x", false, w, 2);
}

static void test_empty_single_quote(void)
{
    const char *w[] = {"", "y"};
    check("'' y", false, w, 2);
}

static void test_empty_quote_at_end(void)
{
    const char *w[] = {""};
    check("\"\"", false, w, 1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_basic);
    RUN_TEST(test_whitespace);
    RUN_TEST(test_quotes);
    RUN_TEST(test_comments);
    RUN_TEST(test_empty);
    RUN_TEST(test_empty_double_quote);
    RUN_TEST(test_empty_single_quote);
    RUN_TEST(test_empty_quote_at_end);
    return UNITY_END();
}
