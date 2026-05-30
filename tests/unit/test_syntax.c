#include "config.h"
#include "syntax.h"
#include "unity.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static struct config base_cfg(void)
{
    struct config c;
    memset(&c, 0, sizeof c);
    return c;
}

static const char *detect(const struct config *c, const char *name,
                          const char *first)
{
    return mat_syntax_detect(c, name, (const unsigned char *)first,
                             first ? strlen(first) : 0);
}

static void test_extension(void)
{
    struct config c = base_cfg();
    TEST_ASSERT_EQUAL_STRING("C", detect(&c, "foo.c", ""));
    TEST_ASSERT_EQUAL_STRING("Python", detect(&c, "a/b/foo.py", ""));
    TEST_ASSERT_EQUAL_STRING("JSON", detect(&c, "pkg.json", ""));
}

static void test_whole_name(void)
{
    struct config c = base_cfg();
    TEST_ASSERT_EQUAL_STRING("Makefile", detect(&c, "src/Makefile", ""));
    TEST_ASSERT_EQUAL_STRING("Dockerfile", detect(&c, "Dockerfile", ""));
}

static void test_shebang(void)
{
    struct config c = base_cfg();
    TEST_ASSERT_EQUAL_STRING("Bash", detect(&c, "script", "#!/bin/bash\n"));
    TEST_ASSERT_EQUAL_STRING("Python",
                             detect(&c, "script", "#!/usr/bin/env python3\n"));
}

static void test_language_override(void)
{
    struct config c = base_cfg();
    c.language = "Rust";
    /* -l wins over the .py extension */
    TEST_ASSERT_EQUAL_STRING("Rust", detect(&c, "foo.py", ""));
}

static void test_map_syntax(void)
{
    struct config c = base_cfg();
    c.map_glob[0] = "*.foo";
    c.map_syntax[0] = "Bar";
    c.nmaps = 1;
    TEST_ASSERT_EQUAL_STRING("Bar", detect(&c, "thing.foo", ""));
    /* map wins over extension table for a matching glob */
    c.map_glob[1] = "*.c";
    c.map_syntax[1] = "MyC";
    c.nmaps = 2;
    TEST_ASSERT_EQUAL_STRING("MyC", detect(&c, "foo.c", ""));
}

static void test_ignored_suffix(void)
{
    struct config c = base_cfg();
    c.ignored_suffix[0] = ".bak";
    c.nsuffix = 1;
    TEST_ASSERT_EQUAL_STRING("C", detect(&c, "foo.c.bak", ""));
}

static void test_fallback(void)
{
    struct config c = base_cfg();
    TEST_ASSERT_EQUAL_STRING("plain", detect(&c, "foo.unknownext", ""));
    c.fallback_syntax = "MyLang";
    TEST_ASSERT_EQUAL_STRING("MyLang", detect(&c, "foo.unknownext", ""));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_extension);
    RUN_TEST(test_whole_name);
    RUN_TEST(test_shebang);
    RUN_TEST(test_language_override);
    RUN_TEST(test_map_syntax);
    RUN_TEST(test_ignored_suffix);
    RUN_TEST(test_fallback);
    return UNITY_END();
}
