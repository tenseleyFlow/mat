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

static enum mat_tok tok_at(struct mat_span *sp, int n, unsigned off)
{
    for (int i = 0; i < n; i++)
        if (off >= sp[i].start && off < sp[i].start + sp[i].len)
            return sp[i].tok;
    return MT_NTOKENS;
}

/* ---- C-family ---- */

static void test_c_line_comment(void)
{
    struct mat_hl *h = mat_hl_open("C");
    struct mat_span sp[MAXSPANS];
    const char *line = "int x; // comment";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_TYPE, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 10));
    mat_hl_close(h);
}

static void test_c_block_comment_multiline(void)
{
    struct mat_hl *h = mat_hl_open("C");
    struct mat_span sp[MAXSPANS];
    const char *l1 = "int x; /* start";
    const char *l2 = "continued */ int y;";
    int n1 =
        mat_hl_line(h, (const unsigned char *)l1, strlen(l1), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 7));

    int n2 =
        mat_hl_line(h, (const unsigned char *)l2, strlen(l2), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n2, 0));
    TEST_ASSERT_EQUAL_INT(MT_TYPE, tok_at(sp, n2, 13));
    mat_hl_close(h);
}

static void test_c_preproc(void)
{
    struct mat_hl *h = mat_hl_open("C");
    struct mat_span sp[MAXSPANS];
    const char *line = "#include <stdio.h>";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 0));
    mat_hl_close(h);
}

static void test_c_string_escape(void)
{
    struct mat_hl *h = mat_hl_open("C");
    struct mat_span sp[MAXSPANS];
    const char *line = "char *s = \"hello\\n\";";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 12));
    mat_hl_close(h);
}

static void test_c_function_detection(void)
{
    struct mat_hl *h = mat_hl_open("C");
    struct mat_span sp[MAXSPANS];
    const char *line = "printf(\"hi\");";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 0));
    mat_hl_close(h);
}

/* ---- Python ---- */

static void test_py_comment(void)
{
    struct mat_hl *h = mat_hl_open("Python");
    struct mat_span sp[MAXSPANS];
    const char *line = "x = 1  # comment";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 9));
    mat_hl_close(h);
}

static void test_py_triple_quote_multiline(void)
{
    struct mat_hl *h = mat_hl_open("Python");
    struct mat_span sp[MAXSPANS];
    const char *l1 = "x = \"\"\"start";
    const char *l2 = "still string\"\"\"";
    int n1 =
        mat_hl_line(h, (const unsigned char *)l1, strlen(l1), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n1, 4));

    int n2 =
        mat_hl_line(h, (const unsigned char *)l2, strlen(l2), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n2, 0));
    mat_hl_close(h);
}

static void test_py_triple_single_quote_multiline(void)
{
    struct mat_hl *h = mat_hl_open("Python");
    struct mat_span sp[MAXSPANS];
    const char *l1 = "x = '''start";
    const char *l2 = "still string'''";
    const char *l3 = "print(x)";
    int n1 =
        mat_hl_line(h, (const unsigned char *)l1, strlen(l1), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n1, 4));

    int n2 =
        mat_hl_line(h, (const unsigned char *)l2, strlen(l2), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n2, 0));

    /* After closing ''', the next line must NOT be string. */
    int n3 =
        mat_hl_line(h, (const unsigned char *)l3, strlen(l3), sp, MAXSPANS);
    TEST_ASSERT_NOT_EQUAL(MT_STRING, tok_at(sp, n3, 0));
    mat_hl_close(h);
}

static void test_py_decorator(void)
{
    struct mat_hl *h = mat_hl_open("Python");
    struct mat_span sp[MAXSPANS];
    const char *line = "@staticmethod";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 0));
    mat_hl_close(h);
}

static void test_py_keyword_type(void)
{
    struct mat_hl *h = mat_hl_open("Python");
    struct mat_span sp[MAXSPANS];
    const char *line = "def foo(): return True";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 11));
    TEST_ASSERT_EQUAL_INT(MT_TYPE, tok_at(sp, n, 18));
    mat_hl_close(h);
}

/* ---- Shell ---- */

static void test_sh_comment(void)
{
    struct mat_hl *h = mat_hl_open("Bash");
    struct mat_span sp[MAXSPANS];
    const char *line = "echo hi # comment";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 10));
    mat_hl_close(h);
}

static void test_sh_variable(void)
{
    struct mat_hl *h = mat_hl_open("Bash");
    struct mat_span sp[MAXSPANS];
    const char *line = "echo $HOME";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 5));
    mat_hl_close(h);
}

static void test_sh_single_quote(void)
{
    struct mat_hl *h = mat_hl_open("Bash");
    struct mat_span sp[MAXSPANS];
    const char *line = "echo 'no $expansion'";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 5));
    mat_hl_close(h);
}

/* ---- Fortran ---- */

static void test_fortran_case_insensitive(void)
{
    struct mat_hl *h = mat_hl_open("Fortran");
    struct mat_span sp[MAXSPANS];
    const char *line = "PROGRAM test";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));

    const char *line2 = "program test";
    n = mat_hl_line(h, (const unsigned char *)line2, strlen(line2), sp,
                    MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    mat_hl_close(h);
}

static void test_fortran_comment(void)
{
    struct mat_hl *h = mat_hl_open("Fortran");
    struct mat_span sp[MAXSPANS];
    const char *line = "x = 1 ! comment";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 8));
    mat_hl_close(h);
}

/* ---- HTML ---- */

static void test_html_tag(void)
{
    struct mat_hl *h = mat_hl_open("HTML");
    struct mat_span sp[MAXSPANS];
    const char *line = "<div class=\"main\">";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 12));
    mat_hl_close(h);
}

static void test_html_comment_multiline(void)
{
    struct mat_hl *h = mat_hl_open("HTML");
    struct mat_span sp[MAXSPANS];
    const char *l1 = "<!-- start";
    const char *l2 = "end --><p>";
    int n1 =
        mat_hl_line(h, (const unsigned char *)l1, strlen(l1), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 0));

    int n2 =
        mat_hl_line(h, (const unsigned char *)l2, strlen(l2), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n2, 0));
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n2, 7));
    mat_hl_close(h);
}

/* ---- Markdown ---- */

static void test_md_heading(void)
{
    struct mat_hl *h = mat_hl_open("Markdown");
    struct mat_span sp[MAXSPANS];
    const char *line = "## Section Title";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    mat_hl_close(h);
}

static void test_md_fenced_code_multiline(void)
{
    struct mat_hl *h = mat_hl_open("Markdown");
    struct mat_span sp[MAXSPANS];
    const char *l1 = "```python";
    const char *l2 = "print('hi')";
    const char *l3 = "```";
    int n1 =
        mat_hl_line(h, (const unsigned char *)l1, strlen(l1), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n1, 0));

    int n2 =
        mat_hl_line(h, (const unsigned char *)l2, strlen(l2), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n2, 0));

    int n3 =
        mat_hl_line(h, (const unsigned char *)l3, strlen(l3), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n3, 0));
    mat_hl_close(h);
}

static void test_md_inline_code(void)
{
    struct mat_hl *h = mat_hl_open("Markdown");
    struct mat_span sp[MAXSPANS];
    const char *line = "Use `mat` here";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 4));
    mat_hl_close(h);
}

/* ---- SQL ---- */

static void test_sql_case_insensitive(void)
{
    struct mat_hl *h = mat_hl_open("SQL");
    struct mat_span sp[MAXSPANS];
    const char *line = "SELECT id FROM users";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 10));
    mat_hl_close(h);
}

static void test_sql_line_comment(void)
{
    struct mat_hl *h = mat_hl_open("SQL");
    struct mat_span sp[MAXSPANS];
    const char *line = "-- comment";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 0));
    mat_hl_close(h);
}

static void test_sql_block_comment_multiline(void)
{
    struct mat_hl *h = mat_hl_open("SQL");
    struct mat_span sp[MAXSPANS];
    const char *l1 = "SELECT /* start";
    const char *l2 = "end */ id";
    int n1 =
        mat_hl_line(h, (const unsigned char *)l1, strlen(l1), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 9));

    int n2 =
        mat_hl_line(h, (const unsigned char *)l2, strlen(l2), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n2, 0));
    mat_hl_close(h);
}

/* ---- Haskell ---- */

static void test_hs_line_comment(void)
{
    struct mat_hl *h = mat_hl_open("Haskell");
    struct mat_span sp[MAXSPANS];
    const char *line = "main = putStrLn -- comment";
    int n =
        mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 19));
    mat_hl_close(h);
}

static void test_hs_block_comment_multiline(void)
{
    struct mat_hl *h = mat_hl_open("Haskell");
    struct mat_span sp[MAXSPANS];
    const char *l1 = "x = 1 {- start";
    const char *l2 = "end -} y = 2";
    int n1 =
        mat_hl_line(h, (const unsigned char *)l1, strlen(l1), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 6));

    int n2 =
        mat_hl_line(h, (const unsigned char *)l2, strlen(l2), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n2, 0));
    mat_hl_close(h);
}

/* ---- Diff (also verifies OOB fix from SR-01 T2) ---- */

static void test_diff_plus_minus(void)
{
    struct mat_hl *h = mat_hl_open("Diff");
    struct mat_span sp[MAXSPANS];
    const char *add = "+added line";
    int n =
        mat_hl_line(h, (const unsigned char *)add, strlen(add), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 0));

    const char *del = "-removed line";
    n = mat_hl_line(h, (const unsigned char *)del, strlen(del), sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    mat_hl_close(h);
}

static void test_diff_short_lines(void)
{
    struct mat_hl *h = mat_hl_open("Diff");
    struct mat_span sp[MAXSPANS];

    const char *d4 = "diff";
    int n = mat_hl_line(h, (const unsigned char *)d4, 4, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 0));

    const char *idx = "index";
    n = mat_hl_line(h, (const unsigned char *)idx, 5, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 0));

    const char *short3 = "ind";
    n = mat_hl_line(h, (const unsigned char *)short3, 3, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_TEXT, tok_at(sp, n, 0));
    mat_hl_close(h);
}

int main(void)
{
    UNITY_BEGIN();
    /* C-family */
    RUN_TEST(test_c_line_comment);
    RUN_TEST(test_c_block_comment_multiline);
    RUN_TEST(test_c_preproc);
    RUN_TEST(test_c_string_escape);
    RUN_TEST(test_c_function_detection);
    /* Python */
    RUN_TEST(test_py_comment);
    RUN_TEST(test_py_triple_quote_multiline);
    RUN_TEST(test_py_triple_single_quote_multiline);
    RUN_TEST(test_py_decorator);
    RUN_TEST(test_py_keyword_type);
    /* Shell */
    RUN_TEST(test_sh_comment);
    RUN_TEST(test_sh_variable);
    RUN_TEST(test_sh_single_quote);
    /* Fortran */
    RUN_TEST(test_fortran_case_insensitive);
    RUN_TEST(test_fortran_comment);
    /* HTML */
    RUN_TEST(test_html_tag);
    RUN_TEST(test_html_comment_multiline);
    /* Markdown */
    RUN_TEST(test_md_heading);
    RUN_TEST(test_md_fenced_code_multiline);
    RUN_TEST(test_md_inline_code);
    /* SQL */
    RUN_TEST(test_sql_case_insensitive);
    RUN_TEST(test_sql_line_comment);
    RUN_TEST(test_sql_block_comment_multiline);
    /* Haskell */
    RUN_TEST(test_hs_line_comment);
    RUN_TEST(test_hs_block_comment_multiline);
    /* Diff */
    RUN_TEST(test_diff_plus_minus);
    RUN_TEST(test_diff_short_lines);
    return UNITY_END();
}
