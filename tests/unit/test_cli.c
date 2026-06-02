#include "unity.h"
#include "cli.h"
#include "config.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static struct config fresh(void)
{
    struct config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.tab_width = -1;
    cfg.squeeze_limit = 1;
    return cfg;
}

static void test_dashdash_stops_options(void)
{
    struct config cfg = fresh();
    const char *files[4];
    char *argv[] = {(char *)"mat", (char *)"--", (char *)"-n", NULL};
    int rc = mat_cli_parse(3, argv, &cfg, files);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, (int)cfg.nfiles);
    TEST_ASSERT_EQUAL_STRING("-n", cfg.files[0]);
    TEST_ASSERT_EQUAL_INT(0, cfg.xform);
}

static void test_attached_range(void)
{
    struct config cfg = fresh();
    const char *files[4];
    char *argv[] = {(char *)"mat", (char *)"-r3:5", NULL};
    int rc = mat_cli_parse(2, argv, &cfg, files);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, (int)cfg.ranges.n);
}

static void test_detached_range(void)
{
    struct config cfg = fresh();
    const char *files[4];
    char *argv[] = {(char *)"mat", (char *)"-r", (char *)"3:5", NULL};
    int rc = mat_cli_parse(3, argv, &cfg, files);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, (int)cfg.ranges.n);
}

static void test_long_equals(void)
{
    struct config cfg = fresh();
    const char *files[4];
    char *argv[] = {(char *)"mat", (char *)"--style=numbers", NULL};
    int rc = mat_cli_parse(2, argv, &cfg, files);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT(cfg.style & MAT_S_NUMBERS);
}

static void test_long_separate(void)
{
    struct config cfg = fresh();
    const char *files[4];
    char *argv[] = {(char *)"mat", (char *)"--style", (char *)"numbers", NULL};
    int rc = mat_cli_parse(3, argv, &cfg, files);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT(cfg.style & MAT_S_NUMBERS);
}

static void test_cat_flags(void)
{
    struct config cfg = fresh();
    const char *files[4];
    char *argv[] = {(char *)"mat", (char *)"-nbs", NULL};
    int rc = mat_cli_parse(2, argv, &cfg, files);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT(cfg.xform & MAT_X_NUMBER);
    TEST_ASSERT(cfg.xform & MAT_X_SQUEEZE);
}

static void test_unknown_option(void)
{
    struct config cfg = fresh();
    const char *files[4];
    char *argv[] = {(char *)"mat", (char *)"--bogus", NULL};
    int rc = mat_cli_parse(2, argv, &cfg, files);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dashdash_stops_options);
    RUN_TEST(test_attached_range);
    RUN_TEST(test_detached_range);
    RUN_TEST(test_long_equals);
    RUN_TEST(test_long_separate);
    RUN_TEST(test_cat_flags);
    RUN_TEST(test_unknown_option);
    return UNITY_END();
}
