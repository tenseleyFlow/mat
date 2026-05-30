#include "range.h"
#include "unity.h"

#include <limits.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* Parse one expression into a fresh set; assert it succeeds. */
static struct mat_rangeset parse_ok(const char *arg)
{
    struct mat_rangeset rs;
    mat_rangeset_init(&rs);
    char err[64];
    TEST_ASSERT_EQUAL_INT(0, mat_range_parse(&rs, arg, err, sizeof err));
    return rs;
}

/* Render the selected lines of [1..total] as a string of '1'/'0' for easy
 * table assertions. */
static void mask(const struct mat_rangeset *rs, long total, char *out)
{
    for (long i = 1; i <= total; i++)
        out[i - 1] = mat_rangeset_contains(rs, i, total) ? '1' : '0';
    out[total] = '\0';
}

static void test_n_m(void)
{
    struct mat_rangeset rs = parse_ok("3:5");
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("00111000", m);
}

static void test_to_m(void)
{
    struct mat_rangeset rs = parse_ok(":4");
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("11110000", m);
}

static void test_n_to_end(void)
{
    struct mat_rangeset rs = parse_ok("5:");
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("00001111", m);
    TEST_ASSERT_TRUE(mat_rangeset_max_line(&rs) == LONG_MAX);
}

static void test_last_n(void)
{
    struct mat_rangeset rs = parse_ok("-3:");
    TEST_ASSERT_TRUE(rs.needs_total);
    TEST_ASSERT_EQUAL_INT(3, rs.max_tail);
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("00000111", m);
    /* clamps when the file is shorter than N */
    mask(&rs, 2, m);
    TEST_ASSERT_EQUAL_STRING("11", m);
}

static void test_n_plus_m(void)
{
    struct mat_rangeset rs = parse_ok("3:+2"); /* 3..5 */
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("00111000", m);
}

static void test_n_context(void)
{
    struct mat_rangeset rs = parse_ok("5::2"); /* 3..7 */
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("00111110", m);
    /* low end clamps to 1 */
    struct mat_rangeset rs2 = parse_ok("2::5"); /* 1..7 */
    mask(&rs2, 8, m);
    TEST_ASSERT_EQUAL_STRING("11111110", m);
}

static void test_single_line(void)
{
    struct mat_rangeset rs = parse_ok("4");
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("00010000", m);
    TEST_ASSERT_EQUAL_INT(4, mat_rangeset_max_line(&rs));
}

static void test_multi_accumulate(void)
{
    struct mat_rangeset rs;
    mat_rangeset_init(&rs);
    char err[64];
    TEST_ASSERT_EQUAL_INT(0, mat_range_parse(&rs, "1:2", err, sizeof err));
    TEST_ASSERT_EQUAL_INT(0, mat_range_parse(&rs, "6:7", err, sizeof err));
    char m[16];
    mask(&rs, 8, m);
    TEST_ASSERT_EQUAL_STRING("11000110", m);
    TEST_ASSERT_EQUAL_INT(7, mat_rangeset_max_line(&rs));
}

static void test_empty_selects_all(void)
{
    struct mat_rangeset rs;
    mat_rangeset_init(&rs);
    TEST_ASSERT_TRUE(mat_rangeset_contains(&rs, 1, 100));
    TEST_ASSERT_TRUE(mat_rangeset_contains(&rs, 100, 100));
    TEST_ASSERT_TRUE(mat_rangeset_max_line(&rs) == LONG_MAX);
}

static void test_errors(void)
{
    struct mat_rangeset rs;
    char err[64];
    const char *bad[] = {"",   ":",   "abc", "0:5", "3:x",
                         "-:", "-0:", "1:2x", "::3", "+3"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        mat_rangeset_init(&rs);
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            -1, mat_range_parse(&rs, bad[i], err, sizeof err), bad[i]);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_n_m);
    RUN_TEST(test_to_m);
    RUN_TEST(test_n_to_end);
    RUN_TEST(test_last_n);
    RUN_TEST(test_n_plus_m);
    RUN_TEST(test_n_context);
    RUN_TEST(test_single_line);
    RUN_TEST(test_multi_accumulate);
    RUN_TEST(test_empty_selects_all);
    RUN_TEST(test_errors);
    return UNITY_END();
}
