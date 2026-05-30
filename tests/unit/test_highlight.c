#include "highlight.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

#define MAXSPANS 256

/* Lex a line and return the span covering byte offset `off`, or MT_NTOKENS. */
static enum mat_tok tok_at(struct mat_span *sp, int n, unsigned off)
{
    for (int i = 0; i < n; i++)
        if (off >= sp[i].start && off < sp[i].start + sp[i].len)
            return sp[i].tok;
    return MT_NTOKENS;
}

static void test_open_close(void)
{
    TEST_ASSERT_NULL(mat_hl_open("Nonexistent Language"));
    TEST_ASSERT_NULL(mat_hl_open(NULL));
    struct mat_hl *h = mat_hl_open("JSON");
    TEST_ASSERT_NOT_NULL(h);
    mat_hl_close(h);
}

static void test_json_basic(void)
{
    struct mat_hl *h = mat_hl_open("JSON");
    const char *line = "{\"key\": 42, \"ok\": true}";
    struct mat_span sp[MAXSPANS];
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_GREATER_THAN_INT(0, n);
    TEST_ASSERT_EQUAL_INT(MT_PUNCT, tok_at(sp, n, 0));     /* { */
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 2));    /* "key" */
    TEST_ASSERT_EQUAL_INT(MT_OPERATOR, tok_at(sp, n, 6));  /* : */
    TEST_ASSERT_EQUAL_INT(MT_NUMBER, tok_at(sp, n, 8));    /* 42 */
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 13));   /* "ok" */
    TEST_ASSERT_EQUAL_INT(MT_CONSTANT, tok_at(sp, n, 18)); /* true */
    mat_hl_close(h);
}

static void test_string_escape(void)
{
    struct mat_hl *h = mat_hl_open("JSON");
    const char *line = "\"a\\\"b\""; /* "a\"b" — escaped quote stays inside */
    struct mat_span sp[MAXSPANS];
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(MT_STRING, sp[0].tok);
    TEST_ASSERT_EQUAL_UINT(strlen(line), sp[0].len);
    mat_hl_close(h);
}

static void test_number_and_constants(void)
{
    struct mat_hl *h = mat_hl_open("JSON");
    const char *line = "-3.14e10 false null";
    struct mat_span sp[MAXSPANS];
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_NUMBER, tok_at(sp, n, 0));    /* -3.14e10 */
    TEST_ASSERT_EQUAL_INT(MT_CONSTANT, tok_at(sp, n, 9));  /* false */
    TEST_ASSERT_EQUAL_INT(MT_CONSTANT, tok_at(sp, n, 15)); /* null */
    mat_hl_close(h);
}

static void test_long_line_guard(void)
{
    struct mat_hl *h = mat_hl_open("JSON");
    size_t big = 20000;
    char *line = malloc(big);
    memset(line, '0', big); /* would otherwise be one huge NUMBER span */
    struct mat_span sp[MAXSPANS];
    int n = mat_hl_line(h, (const unsigned char *)line, big, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(MT_TEXT, sp[0].tok);
    TEST_ASSERT_EQUAL_UINT(big, sp[0].len);
    free(line);
    mat_hl_close(h);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_open_close);
    RUN_TEST(test_json_basic);
    RUN_TEST(test_string_escape);
    RUN_TEST(test_number_and_constants);
    RUN_TEST(test_long_line_guard);
    return UNITY_END();
}
