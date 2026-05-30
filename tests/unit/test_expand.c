#include "unity.h"
#include "expand.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* helper: expand one byte and return it as a length-checked C string */
static void expect(unsigned char ch, bool show_tabs, const char *want)
{
    unsigned char out[4];
    int n = mat_expand_byte(ch, show_tabs, out);
    TEST_ASSERT_EQUAL_INT((int)strlen(want), n);
    TEST_ASSERT_EQUAL_MEMORY(want, out, (size_t)n);
}

static void test_printable_ascii(void)
{
    expect('A', false, "A");
    expect(' ', false, " ");
    expect('~', false, "~"); /* 126 */
}

static void test_tab(void)
{
    expect('\t', false, "\t"); /* literal when tabs not shown */
    expect('\t', true, "^I");  /* ^I when -t/-T */
}

static void test_control_chars(void)
{
    expect(0, false, "^@");
    expect(1, false, "^A");
    expect(31, false, "^_");
    expect(127, false, "^?");
    expect('\n', false, "\n"); /* newline maps to itself */
}

static void test_high_bytes(void)
{
    expect(128, false, "M-^@"); /* 128: M- + ^@ */
    expect(159, false, "M-^_"); /* 159: M- + ^_ */
    expect(160, false, "M- ");  /* 160: M- + space */
    expect(200, false, "M-H");  /* 200-128 = 72 = 'H' */
    expect(254, false, "M-~");  /* 254-128 = 126 = '~' */
    expect(255, false, "M-^?"); /* 255: M- + ^? */
}

static void test_table_matches_function(void)
{
    struct mat_xtable t;
    mat_xtable_build(&t, true);
    for (int i = 0; i < 256; i++) {
        unsigned char out[4];
        int n = mat_expand_byte((unsigned char)i, true, out);
        TEST_ASSERT_EQUAL_INT(n, t.len[i]);
        TEST_ASSERT_EQUAL_MEMORY(out, t.buf[i], (size_t)n);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_printable_ascii);
    RUN_TEST(test_tab);
    RUN_TEST(test_control_chars);
    RUN_TEST(test_high_bytes);
    RUN_TEST(test_table_matches_function);
    return UNITY_END();
}
