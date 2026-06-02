#include "unity.h"
#include "highlight.h"
#include "render.h"
#include "width.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static char out_buf[4096];
static size_t out_len;

static void capture_sink(void *ctx, const char *bytes, size_t len)
{
    (void)ctx;
    if (out_len + len < sizeof out_buf) {
        memcpy(out_buf + out_len, bytes, len);
        out_len += len;
    }
}

static int render(const char *lang, const char *line, int tab_width)
{
    struct mat_render r;
    mat_render_init(&r, MAT_S_NUMBERS, MAT_WRAP_NEVER, tab_width, true);
    r.hl = mat_hl_open(lang);
    out_len = 0;
    int segs = mat_render_line(&r, 1, (const unsigned char *)line, strlen(line),
                               80, capture_sink, NULL);
    out_buf[out_len] = '\0';
    mat_hl_close(r.hl);
    mat_render_free(&r);
    return segs;
}

static void test_ascii_tab_keyword(void)
{
    render("C", "\tint x;", 4);
    TEST_ASSERT_GREATER_THAN_UINT(0, out_len);
    TEST_ASSERT_NOT_NULL(strstr(out_buf, "int"));
}

static void test_cjk_tab_keyword(void)
{
    /* U+4E2D (3 bytes, width 2) + tab + "int x;" */
    render("C", "\xe4\xb8\xad\tint x;", 8);
    TEST_ASSERT_GREATER_THAN_UINT(0, out_len);
    TEST_ASSERT_NOT_NULL(strstr(out_buf, "int"));
}

static void test_multi_tab(void)
{
    render("C", "\t\tint x;", 4);
    TEST_ASSERT_GREATER_THAN_UINT(0, out_len);
    TEST_ASSERT_NOT_NULL(strstr(out_buf, "int"));
}

static void test_tab_at_pos_zero(void)
{
    render("Makefile", "\techo hello", 4);
    TEST_ASSERT_GREATER_THAN_UINT(0, out_len);
}

static void test_no_tabs(void)
{
    render("C", "int x = 42;", 4);
    TEST_ASSERT_GREATER_THAN_UINT(0, out_len);
    TEST_ASSERT_NOT_NULL(strstr(out_buf, "int"));
}

static void test_tab_inside_span(void)
{
    /* Makefile: "\techo\thello" — the whole line is MT_STRING because it
     * starts with a tab. The span covers bytes containing tabs, so the
     * remap must expand tabs inside the span without corrupting offsets. */
    render("Makefile", "\techo\thello", 4);
    TEST_ASSERT_GREATER_THAN_UINT(0, out_len);
    TEST_ASSERT_NOT_NULL(strstr(out_buf, "echo"));
    TEST_ASSERT_NOT_NULL(strstr(out_buf, "hello"));
}

static void test_c_string_with_tab(void)
{
    /* C string literal containing a tab: "he\tllo" — the lexer emits
     * MT_STRING spanning the full literal including the tab. */
    render("C", "char *s = \"he\tllo\";", 4);
    TEST_ASSERT_GREATER_THAN_UINT(0, out_len);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ascii_tab_keyword);
    RUN_TEST(test_cjk_tab_keyword);
    RUN_TEST(test_multi_tab);
    RUN_TEST(test_tab_at_pos_zero);
    RUN_TEST(test_no_tabs);
    RUN_TEST(test_tab_inside_span);
    RUN_TEST(test_c_string_with_tab);
    return UNITY_END();
}
