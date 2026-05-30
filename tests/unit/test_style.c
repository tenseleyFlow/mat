#include "unity.h"
#include "style.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static unsigned parse_ok(const char *spec)
{
    unsigned out = 0xdead;
    char err[64];
    TEST_ASSERT_EQUAL_INT(0, mat_style_parse(spec, &out, err, sizeof err));
    return out;
}

static void test_presets(void)
{
    TEST_ASSERT_EQUAL_UINT(0u, parse_ok("plain"));
    TEST_ASSERT_EQUAL_UINT((unsigned)MAT_STYLE_FULL, parse_ok("full"));
    TEST_ASSERT_EQUAL_UINT((unsigned)MAT_STYLE_DEFAULT, parse_ok("default"));
    TEST_ASSERT_EQUAL_UINT((unsigned)MAT_STYLE_DEFAULT, parse_ok("auto"));
}

static void test_components(void)
{
    TEST_ASSERT_EQUAL_UINT(MAT_S_NUMBERS, parse_ok("numbers"));
    TEST_ASSERT_EQUAL_UINT(MAT_S_NUMBERS | MAT_S_GRID,
                           parse_ok("numbers,grid"));
    TEST_ASSERT_EQUAL_UINT(MAT_S_HEADER, parse_ok("header"));
    TEST_ASSERT_EQUAL_UINT(MAT_S_HEADER, parse_ok("header-filename"));
    TEST_ASSERT_EQUAL_UINT(MAT_S_HEADER_SIZE, parse_ok("header-filesize"));
}

static void test_modifiers(void)
{
    /* bare first token replaces */
    TEST_ASSERT_EQUAL_UINT(MAT_S_NUMBERS | MAT_S_GRID,
                           parse_ok("grid,numbers"));
    /* preset then subtract */
    TEST_ASSERT_EQUAL_UINT((unsigned)MAT_STYLE_FULL &
                               ~(unsigned)MAT_S_HEADER_SIZE,
                           parse_ok("full,-header-filesize"));
    /* leading +/- modifies the default set */
    TEST_ASSERT_EQUAL_UINT((unsigned)MAT_STYLE_DEFAULT | MAT_S_NUMBERS,
                           parse_ok("+numbers"));
    TEST_ASSERT_EQUAL_UINT((unsigned)MAT_STYLE_DEFAULT & ~(unsigned)MAT_S_GRID,
                           parse_ok("-grid"));
}

static void test_unknown(void)
{
    unsigned out;
    char err[64];
    TEST_ASSERT_EQUAL_INT(
        -1, mat_style_parse("numbers,bogus", &out, err, sizeof err));
    TEST_ASSERT_EQUAL_STRING("bogus", err);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_presets);
    RUN_TEST(test_components);
    RUN_TEST(test_modifiers);
    RUN_TEST(test_unknown);
    return UNITY_END();
}
