#include "highlight.h"
#include "unity.h"

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

#define LEX(syntax, line)                                                      \
    struct mat_hl *h = mat_hl_open(syntax);                                    \
    TEST_ASSERT_NOT_NULL(h);                                                   \
    struct mat_span sp[MAXSPANS];                                              \
    int n = mat_hl_line(h, (const unsigned char *)(line), strlen(line), sp,    \
                        MAXSPANS)

#define CLOSE mat_hl_close(h)

/* Perl/PHP: $var, # comment, block comment carry-over */
static void test_perish_var(void)
{
    LEX("Perl", "my $x = 1; # comment");
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 3));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 13));
    CLOSE;
}

static void test_perish_block_comment(void)
{
    struct mat_hl *h = mat_hl_open("PHP");
    TEST_ASSERT_NOT_NULL(h);
    struct mat_span sp[MAXSPANS];
    int n1 = mat_hl_line(h, (const unsigned char *)"$x = 1; /* start", 17, sp,
                         MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 8));
    int n2 =
        mat_hl_line(h, (const unsigned char *)"end */ $y", 9, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n2, 0));
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n2, 7));
    CLOSE;
}

/* Clojure: ; comment, :keyword */
static void test_clojure(void)
{
    LEX("Clojure", "(defn foo [x] :bar) ; comment");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 1));
    TEST_ASSERT_EQUAL_INT(MT_CONSTANT, tok_at(sp, n, 14));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 22));
    CLOSE;
}

/* R: # comment, <- operator */
static void test_r(void)
{
    LEX("R", "x <- 42 # comment");
    TEST_ASSERT_EQUAL_INT(MT_OPERATOR, tok_at(sp, n, 2));
    TEST_ASSERT_EQUAL_INT(MT_NUMBER, tok_at(sp, n, 5));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 10));
    CLOSE;
}

/* Dockerfile: FROM keyword, $VAR */
static void test_dockerfile(void)
{
    LEX("Dockerfile", "FROM ubuntu:22.04");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    CLOSE;
}

static void test_dockerfile_var(void)
{
    LEX("Dockerfile", "RUN echo $HOME");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 9));
    CLOSE;
}

/* INI: [section], key=value, ; comment */
static void test_ini(void)
{
    struct mat_hl *h = mat_hl_open("INI");
    TEST_ASSERT_NOT_NULL(h);
    struct mat_span sp[MAXSPANS];
    int n1 =
        mat_hl_line(h, (const unsigned char *)"; comment", 9, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 0));
    int n2 =
        mat_hl_line(h, (const unsigned char *)"[section]", 9, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n2, 0));
    int n3 =
        mat_hl_line(h, (const unsigned char *)"key=value", 9, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_TYPE, tok_at(sp, n3, 0));
    TEST_ASSERT_EQUAL_INT(MT_OPERATOR, tok_at(sp, n3, 3));
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n3, 4));
    CLOSE;
}

/* LaTeX: \command, % comment, $math$ */
static void test_latex(void)
{
    LEX("LaTeX", "\\section{Title} % comment");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 18));
    CLOSE;
}

/* Pascal: { } block comment carry-over */
static void test_pascal_block(void)
{
    struct mat_hl *h = mat_hl_open("Pascal");
    TEST_ASSERT_NOT_NULL(h);
    struct mat_span sp[MAXSPANS];
    int n1 = mat_hl_line(h, (const unsigned char *)"x := 1; { start", 16, sp,
                         MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 8));
    int n2 =
        mat_hl_line(h, (const unsigned char *)"end } var y;", 12, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n2, 0));
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n2, 6));
    CLOSE;
}

/* MATLAB: % comment, function call */
static void test_matlab(void)
{
    LEX("MATLAB", "x = linspace(0, 1); % grid");
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 4));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 22));
    CLOSE;
}

/* Assembly: ; comment, .directive */
static void test_asm(void)
{
    LEX("Assembly", ".text ; comment");
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 8));
    CLOSE;
}

/* Batch: REM comment, :: comment, %var% */
static void test_batch_rem(void)
{
    LEX("Batch File", "REM this is a comment");
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 0));
    CLOSE;
}

static void test_batch_var(void)
{
    LEX("Batch File", "echo %NAME%");
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 5));
    CLOSE;
}

/* VimL: " comment */
static void test_viml(void)
{
    LEX("VimL", "set number");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    CLOSE;
}

/* Groff: .TH directive */
static void test_groff(void)
{
    LEX("Groff", ".TH MAT 1");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    CLOSE;
}

/* BibTeX: @article, % comment */
static void test_bibtex(void)
{
    LEX("BibTeX", "@article{key, % note");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 16));
    CLOSE;
}

/* Strace: syscall(args) = result */
static void test_strace(void)
{
    LEX("Strace", "read(3, \"hello\", 5) = 5");
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 8));
    CLOSE;
}

/* log: timestamp, ERROR/INFO/DEBUG */
static void test_log(void)
{
    LEX("log", "2026-05-30T08:00:00Z ERROR connection refused");
    TEST_ASSERT_EQUAL_INT(MT_NUMBER, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 21));
    CLOSE;
}

/* Todo.txt: (A) priority, +project, @context, x completed */
static void test_todotxt(void)
{
    LEX("Todo.txt", "(A) Call Mom +Family @phone");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 13));
    TEST_ASSERT_EQUAL_INT(MT_PREPROC, tok_at(sp, n, 21));
    CLOSE;
}

static void test_todotxt_done(void)
{
    LEX("Todo.txt", "x 2026-05-30 Completed task");
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 0));
    CLOSE;
}

/* VimHelp: *tag*, |link|, > code */
static void test_vimhelp(void)
{
    LEX("VimHelp", "*mat.txt* help |usage|");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 15));
    CLOSE;
}

static void test_vimhelp_code(void)
{
    LEX("VimHelp", "> echo 'code'");
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 0));
    CLOSE;
}

/* HTTP: GET method, Header: value */
static void test_http(void)
{
    LEX("HTTP Request and Response", "GET /api/users HTTP/1.1");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    CLOSE;
}

static void test_http_header(void)
{
    LEX("HTTP Request and Response", "Content-Type: application/json");
    TEST_ASSERT_EQUAL_INT(MT_TYPE, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_OPERATOR, tok_at(sp, n, 12));
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n, 13));
    CLOSE;
}

/* JQ: .field, | pipe, # comment */
static void test_jq(void)
{
    LEX("JQ", ".[] | select(.age > 30) # filter");
    TEST_ASSERT_EQUAL_INT(MT_FUNCTION, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_OPERATOR, tok_at(sp, n, 4));
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 25));
    CLOSE;
}

/* Crontab: timing fields, # comment */
static void test_crontab(void)
{
    struct mat_hl *h = mat_hl_open("Crontab");
    TEST_ASSERT_NOT_NULL(h);
    struct mat_span sp[MAXSPANS];
    int n1 =
        mat_hl_line(h, (const unsigned char *)"# daily job", 11, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 0));
    int n2 = mat_hl_line(h, (const unsigned char *)"0 3 * * * /usr/bin/run", 22,
                         sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_NUMBER, tok_at(sp, n2, 0));
    CLOSE;
}

/* Git Commit: # comment */
static void test_gitcommit(void)
{
    LEX("Git Commit", "# Please enter the commit message");
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n, 0));
    CLOSE;
}

/* Git Rebase Todo: pick keyword, commit hash */
static void test_gitrebase(void)
{
    LEX("Git Rebase Todo", "pick abc1234 initial commit");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_CONSTANT, tok_at(sp, n, 5));
    CLOSE;
}

static void test_gitrebase_empty(void)
{
    struct mat_hl *h = mat_hl_open("Git Rebase Todo");
    TEST_ASSERT_NOT_NULL(h);
    struct mat_span sp[MAXSPANS];
    int n = mat_hl_line(h, (const unsigned char *)"", 0, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(0, n);
    CLOSE;
}

/* SSH Config: keyword value, # comment */
static void test_sshconfig(void)
{
    struct mat_hl *h = mat_hl_open("SSH Config");
    TEST_ASSERT_NOT_NULL(h);
    struct mat_span sp[MAXSPANS];
    int n1 =
        mat_hl_line(h, (const unsigned char *)"# comment", 9, sp, MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 0));
    int n2 = mat_hl_line(h, (const unsigned char *)"Host myserver", 13, sp,
                         MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n2, 0));
    TEST_ASSERT_EQUAL_INT(MT_STRING, tok_at(sp, n2, 5));
    CLOSE;
}

/* Colon file (passwd): field:field:field */
static void test_colonfile(void)
{
    LEX("passwd", "root:x:0:0:root:/root:/bin/bash");
    TEST_ASSERT_EQUAL_INT(MT_KEYWORD, tok_at(sp, n, 0));
    TEST_ASSERT_EQUAL_INT(MT_OPERATOR, tok_at(sp, n, 4));
    CLOSE;
}

/* Literate Haskell: > prefixed code vs prose */
static void test_lhaskell(void)
{
    struct mat_hl *h = mat_hl_open("Literate Haskell");
    TEST_ASSERT_NOT_NULL(h);
    struct mat_span sp[MAXSPANS];
    int n1 = mat_hl_line(h, (const unsigned char *)"This is prose", 13, sp,
                         MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_COMMENT, tok_at(sp, n1, 0));
    int n2 = mat_hl_line(h, (const unsigned char *)"> main = putStrLn", 17, sp,
                         MAXSPANS);
    TEST_ASSERT_EQUAL_INT(MT_OPERATOR, tok_at(sp, n2, 0));
    CLOSE;
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_perish_var);
    RUN_TEST(test_perish_block_comment);
    RUN_TEST(test_clojure);
    RUN_TEST(test_r);
    RUN_TEST(test_dockerfile);
    RUN_TEST(test_dockerfile_var);
    RUN_TEST(test_ini);
    RUN_TEST(test_latex);
    RUN_TEST(test_pascal_block);
    RUN_TEST(test_matlab);
    RUN_TEST(test_asm);
    RUN_TEST(test_batch_rem);
    RUN_TEST(test_batch_var);
    RUN_TEST(test_viml);
    RUN_TEST(test_groff);
    RUN_TEST(test_bibtex);
    RUN_TEST(test_strace);
    RUN_TEST(test_log);
    RUN_TEST(test_todotxt);
    RUN_TEST(test_todotxt_done);
    RUN_TEST(test_vimhelp);
    RUN_TEST(test_vimhelp_code);
    RUN_TEST(test_http);
    RUN_TEST(test_http_header);
    RUN_TEST(test_jq);
    RUN_TEST(test_crontab);
    RUN_TEST(test_gitcommit);
    RUN_TEST(test_gitrebase);
    RUN_TEST(test_gitrebase_empty);
    RUN_TEST(test_sshconfig);
    RUN_TEST(test_colonfile);
    RUN_TEST(test_lhaskell);
    return UNITY_END();
}
