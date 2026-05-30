#include "ansi.h"
#include "unity.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* strip s and compare the result to expected */
static void check(const char *s, const char *expected)
{
    char out[256];
    size_t n = mat_strip_ansi((const unsigned char *)s, strlen(s), out);
    out[n] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, out);
}

static void test_plain_unchanged(void)
{
    check("hello world", "hello world");
}

static void test_sgr_color(void)
{
    check("\x1b[31mred\x1b[0m", "red");
    check("\x1b[1;32mbold green\x1b[0m text", "bold green text");
}

static void test_cursor_moves(void)
{
    check("a\x1b[2Kb\x1b[Hc", "abc");
}

static void test_osc_bel(void)
{
    /* OSC set-title terminated by BEL */
    check("\x1b]0;title\x07keep", "keep");
}

static void test_osc_st(void)
{
    /* OSC terminated by ESC backslash (string terminator) */
    check("\x1b]8;;http://x\x1b\\link", "link");
}

static void test_lone_esc(void)
{
    char out[8];
    size_t n = mat_strip_ansi((const unsigned char *)"a\x1b", 2, out);
    out[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("a", out);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_plain_unchanged);
    RUN_TEST(test_sgr_color);
    RUN_TEST(test_cursor_moves);
    RUN_TEST(test_osc_bel);
    RUN_TEST(test_osc_st);
    RUN_TEST(test_lone_esc);
    return UNITY_END();
}
