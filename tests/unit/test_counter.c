#include "unity.h"
#include "counter.h"

void setUp(void)
{
}
void tearDown(void)
{
}

/* advance the counter to value n and return the formatted string */
static const char *advance_to(struct mat_counter *c, long n)
{
    const char *s = NULL;
    for (long i = 0; i < n; i++)
        s = mat_counter_next(c);
    return s;
}

static void test_first_is_one(void)
{
    struct mat_counter c;
    mat_counter_init(&c);
    TEST_ASSERT_EQUAL_STRING("     1\t", mat_counter_next(&c));
}

static void test_field_width_and_digits(void)
{
    struct mat_counter c;
    mat_counter_init(&c);
    TEST_ASSERT_EQUAL_STRING("     9\t", advance_to(&c, 9));
    TEST_ASSERT_EQUAL_STRING("    10\t", advance_to(&c, 1));     /* -> 10 */
    TEST_ASSERT_EQUAL_STRING("   100\t", advance_to(&c, 90));    /* -> 100 */
    TEST_ASSERT_EQUAL_STRING(" 99999\t", advance_to(&c, 99899)); /* -> 99999 */
    TEST_ASSERT_EQUAL_STRING("999999\t",
                             advance_to(&c, 900000)); /* -> 999999 */
}

/* Past 999999 the 6-wide field widens, matching cat. */
static void test_widening(void)
{
    struct mat_counter c;
    mat_counter_init(&c);
    TEST_ASSERT_EQUAL_STRING("1000000\t", advance_to(&c, 1000000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_is_one);
    RUN_TEST(test_field_width_and_digits);
    RUN_TEST(test_widening);
    return UNITY_END();
}
