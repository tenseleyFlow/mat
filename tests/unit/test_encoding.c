#include "encoding.h"
#include "unity.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static enum mat_encoding sniff(const char *p, size_t n)
{
    return mat_encoding_sniff((const unsigned char *)p, n);
}

static void test_empty(void)
{
    TEST_ASSERT_EQUAL_INT(MAT_ENC_EMPTY, sniff("", 0));
}

static void test_ascii(void)
{
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF8, sniff("hello world\n", 12));
}

static void test_utf8_multibyte(void)
{
    /* "café" — é is C3 A9, no NUL, no BOM */
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF8, sniff("caf\xc3\xa9", 5));
}

static void test_utf8_bom(void)
{
    const unsigned char b[] = {0xEF, 0xBB, 0xBF, 'h', 'i'};
    enum mat_encoding e = mat_encoding_sniff(b, sizeof b);
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF8, e);
    TEST_ASSERT_EQUAL_UINT(3, mat_encoding_bom_len(e, b, sizeof b));
}

static void test_utf16le(void)
{
    const unsigned char b[] = {0xFF, 0xFE, 'A', 0x00, 'B', 0x00};
    enum mat_encoding e = mat_encoding_sniff(b, sizeof b);
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF16LE, e);
    TEST_ASSERT_EQUAL_UINT(2, mat_encoding_bom_len(e, b, sizeof b));
}

static void test_utf16be(void)
{
    const unsigned char b[] = {0xFE, 0xFF, 0x00, 'A', 0x00, 'B'};
    enum mat_encoding e = mat_encoding_sniff(b, sizeof b);
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF16BE, e);
    TEST_ASSERT_EQUAL_UINT(2, mat_encoding_bom_len(e, b, sizeof b));
}

static void test_binary_nul(void)
{
    const unsigned char b[] = {'a', 'b', 0x00, 'c'};
    TEST_ASSERT_EQUAL_INT(MAT_ENC_BINARY, mat_encoding_sniff(b, sizeof b));
}

static void test_utf32_is_binary(void)
{
    const unsigned char le[] = {0xFF, 0xFE, 0x00, 0x00, 'A', 0x00, 0x00, 0x00};
    const unsigned char be[] = {0x00, 0x00, 0xFE, 0xFF, 0x00, 0x00, 0x00, 'A'};
    TEST_ASSERT_EQUAL_INT(MAT_ENC_BINARY, mat_encoding_sniff(le, sizeof le));
    TEST_ASSERT_EQUAL_INT(MAT_ENC_BINARY, mat_encoding_sniff(be, sizeof be));
}

static void test_bom_len_none(void)
{
    TEST_ASSERT_EQUAL_UINT(
        0, mat_encoding_bom_len(MAT_ENC_UTF8, (const unsigned char *)"hi", 2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty);
    RUN_TEST(test_ascii);
    RUN_TEST(test_utf8_multibyte);
    RUN_TEST(test_utf8_bom);
    RUN_TEST(test_utf16le);
    RUN_TEST(test_utf16be);
    RUN_TEST(test_binary_nul);
    RUN_TEST(test_utf32_is_binary);
    RUN_TEST(test_bom_len_none);
    return UNITY_END();
}
