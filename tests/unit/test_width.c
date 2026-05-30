#include "unity.h"
#include "width.h"

void setUp(void)
{
}
void tearDown(void)
{
}

static uint32_t decode(const unsigned char *d, size_t len, size_t *consumed)
{
    uint32_t cp = 0;
    *consumed = mat_utf8_decode(d, d + len, &cp);
    return cp;
}

static void test_decode(void)
{
    size_t n;
    TEST_ASSERT_EQUAL_UINT32('A', decode((const unsigned char *)"A", 1, &n));
    TEST_ASSERT_EQUAL_INT(1, (int)n);

    const unsigned char emdash[] = {0xe2, 0x80, 0x94}; /* U+2014 */
    TEST_ASSERT_EQUAL_HEX32(0x2014, decode(emdash, 3, &n));
    TEST_ASSERT_EQUAL_INT(3, (int)n);

    const unsigned char cjk[] = {0xe4, 0xb8, 0x80}; /* U+4E00 */
    TEST_ASSERT_EQUAL_HEX32(0x4E00, decode(cjk, 3, &n));
    TEST_ASSERT_EQUAL_INT(3, (int)n);

    const unsigned char emoji[] = {0xf0, 0x9f, 0x98, 0x80}; /* U+1F600 */
    TEST_ASSERT_EQUAL_HEX32(0x1F600, decode(emoji, 4, &n));
    TEST_ASSERT_EQUAL_INT(4, (int)n);
}

static void test_decode_invalid(void)
{
    size_t n;
    /* truncated 3-byte sequence -> report first byte, consume 1 */
    const unsigned char trunc[] = {0xe2, 0x80};
    TEST_ASSERT_EQUAL_HEX32(0xe2, decode(trunc, 2, &n));
    TEST_ASSERT_EQUAL_INT(1, (int)n);
    /* lone continuation byte */
    const unsigned char cont[] = {0x80};
    TEST_ASSERT_EQUAL_HEX32(0x80, decode(cont, 1, &n));
    TEST_ASSERT_EQUAL_INT(1, (int)n);
}

static void test_width(void)
{
    TEST_ASSERT_EQUAL_INT(1, mat_wcwidth('A'));
    TEST_ASSERT_EQUAL_INT(1, mat_wcwidth(' '));
    TEST_ASSERT_EQUAL_INT(1, mat_wcwidth(0x2014));  /* em-dash: narrow */
    TEST_ASSERT_EQUAL_INT(2, mat_wcwidth(0x4E00));  /* CJK ideograph */
    TEST_ASSERT_EQUAL_INT(2, mat_wcwidth(0xFF21));  /* fullwidth A */
    TEST_ASSERT_EQUAL_INT(2, mat_wcwidth(0x1F600)); /* emoji */
    TEST_ASSERT_EQUAL_INT(0, mat_wcwidth(0x0301));  /* combining acute */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_decode);
    RUN_TEST(test_decode_invalid);
    RUN_TEST(test_width);
    return UNITY_END();
}
