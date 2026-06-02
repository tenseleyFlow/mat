#include "encoding.h"
#include "linesrc.h"
#include "scan.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static int write_tmp(const void *data, size_t len)
{
    char tmpl[] = "/tmp/mat_ls_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0)
        return -1;
    unlink(tmpl);
    write(fd, data, len);
    lseek(fd, 0, SEEK_SET);
    return fd;
}

static void test_utf16le_ascii(void)
{
    /* BOM + "hi\n" in UTF-16LE */
    unsigned char d[] = {0xFF, 0xFE, 'h', 0, 'i', 0, '\n', 0};
    int fd = write_tmp(d, sizeof d);
    TEST_ASSERT_TRUE(fd >= 0);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF16LE, s.encoding);
    const unsigned char *line;
    size_t len;
    TEST_ASSERT_TRUE(mat_linesrc_line(&s, 0, &line, &len));
    TEST_ASSERT_EQUAL_UINT(2, len);
    TEST_ASSERT_EQUAL_UINT8('h', line[0]);
    TEST_ASSERT_EQUAL_UINT8('i', line[1]);
    mat_linesrc_free(&s);
    close(fd);
}

static void test_utf16be(void)
{
    /* BOM + "AB\n" in UTF-16BE */
    unsigned char d[] = {0xFE, 0xFF, 0, 'A', 0, 'B', 0, '\n'};
    int fd = write_tmp(d, sizeof d);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF16BE, s.encoding);
    const unsigned char *line;
    size_t len;
    TEST_ASSERT_TRUE(mat_linesrc_line(&s, 0, &line, &len));
    TEST_ASSERT_EQUAL_UINT(2, len);
    TEST_ASSERT_EQUAL_UINT8('A', line[0]);
    TEST_ASSERT_EQUAL_UINT8('B', line[1]);
    mat_linesrc_free(&s);
    close(fd);
}

static void test_utf16le_surrogate_pair(void)
{
    /* BOM + U+1F600 (emoji) + \n in UTF-16LE: D83D DE00 */
    unsigned char d[] = {0xFF, 0xFE, 0x3D, 0xD8, 0x00, 0xDE, '\n', 0};
    int fd = write_tmp(d, sizeof d);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    const unsigned char *line;
    size_t len;
    TEST_ASSERT_TRUE(mat_linesrc_line(&s, 0, &line, &len));
    /* U+1F600 in UTF-8: F0 9F 98 80 */
    TEST_ASSERT_EQUAL_UINT(4, len);
    TEST_ASSERT_EQUAL_UINT8(0xF0, line[0]);
    TEST_ASSERT_EQUAL_UINT8(0x9F, line[1]);
    TEST_ASSERT_EQUAL_UINT8(0x98, line[2]);
    TEST_ASSERT_EQUAL_UINT8(0x80, line[3]);
    mat_linesrc_free(&s);
    close(fd);
}

static void test_utf16_bom_only(void)
{
    /* Just a BOM, no content */
    unsigned char d[] = {0xFF, 0xFE};
    int fd = write_tmp(d, sizeof d);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    const unsigned char *line;
    size_t len;
    TEST_ASSERT_FALSE(mat_linesrc_line(&s, 0, &line, &len));
    mat_linesrc_free(&s);
    close(fd);
}

static void test_utf8_bom_stripped(void)
{
    unsigned char d[] = {0xEF, 0xBB, 0xBF, 'h', 'i', '\n'};
    int fd = write_tmp(d, sizeof d);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF8, s.encoding);
    const unsigned char *line;
    size_t len;
    TEST_ASSERT_TRUE(mat_linesrc_line(&s, 0, &line, &len));
    TEST_ASSERT_EQUAL_UINT(2, len);
    TEST_ASSERT_EQUAL_UINT8('h', line[0]);
    mat_linesrc_free(&s);
    close(fd);
}

static void test_utf16le_odd_byte_count(void)
{
    /* BOM + 3 content bytes = truncated last code unit */
    unsigned char d[] = {0xFF, 0xFE, 'a', 0x00, 'b'};
    int fd = write_tmp(d, sizeof d);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    TEST_ASSERT_EQUAL_INT(MAT_ENC_UTF16LE, s.encoding);
    const unsigned char *line;
    size_t len;
    TEST_ASSERT_TRUE(mat_linesrc_line(&s, 0, &line, &len));
    TEST_ASSERT_GREATER_THAN_UINT(0, len);
    mat_linesrc_free(&s);
    close(fd);
}

static void test_utf16le_unpaired_surrogate(void)
{
    /* BOM + high surrogate D83D + newline 000A — no low surrogate */
    unsigned char d[] = {0xFF, 0xFE, 0x3D, 0xD8, 0x0A, 0x00};
    int fd = write_tmp(d, sizeof d);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    const unsigned char *line;
    size_t len;
    TEST_ASSERT_TRUE(mat_linesrc_line(&s, 0, &line, &len));
    /* Should produce U+FFFD replacement char (3 UTF-8 bytes) */
    TEST_ASSERT_GREATER_THAN_UINT(0, len);
    mat_linesrc_free(&s);
    close(fd);
}

static void test_utf16le_bom_plus_garbage(void)
{
    /* BOM + NUL NUL FF FF — edge-case code points */
    unsigned char d[] = {0xFF, 0xFE, 0x00, 0x00, 0xFF, 0xFF};
    int fd = write_tmp(d, sizeof d);
    struct mat_linesrc s;
    TEST_ASSERT_TRUE(mat_linesrc_open(&s, fd));
    mat_linesrc_free(&s);
    close(fd);
}

int main(void)
{
    mat_scan_init();
    UNITY_BEGIN();
    RUN_TEST(test_utf16le_ascii);
    RUN_TEST(test_utf16be);
    RUN_TEST(test_utf16le_surrogate_pair);
    RUN_TEST(test_utf16_bom_only);
    RUN_TEST(test_utf8_bom_stripped);
    RUN_TEST(test_utf16le_odd_byte_count);
    RUN_TEST(test_utf16le_unpaired_surrogate);
    RUN_TEST(test_utf16le_bom_plus_garbage);
    return UNITY_END();
}
