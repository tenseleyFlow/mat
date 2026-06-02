#include "unity.h"
#include "highlight.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

static void test_all_tables_sorted(void)
{
    const char *bad = mat_hl_validate_tables();
    if (bad != NULL)
        TEST_FAIL_MESSAGE(bad);
}

static void test_every_dispatch_entry_opens(void)
{
    const char *const *names;
    int count;
    mat_hl_dispatch_names(&names, &count);
    TEST_ASSERT_GREATER_THAN_INT(100, count);
    for (int i = 0; i < count; i++) {
        struct mat_hl *h = mat_hl_open(names[i]);
        if (h == NULL) {
            char msg[128];
            snprintf(msg, sizeof msg, "mat_hl_open(\"%s\") returned NULL",
                     names[i]);
            TEST_FAIL_MESSAGE(msg);
        }
        mat_hl_close(h);
    }
}

static void test_every_dispatch_entry_lexes(void)
{
    const char *const *names;
    int count;
    mat_hl_dispatch_names(&names, &count);
    struct mat_span sp[64];
    for (int i = 0; i < count; i++) {
        struct mat_hl *h = mat_hl_open(names[i]);
        if (h == NULL)
            continue;
        const char *line = "int x = 42;";
        int n =
            mat_hl_line(h, (const unsigned char *)line, strlen(line), sp, 64);
        TEST_ASSERT_GREATER_OR_EQUAL_INT(0, n);
        mat_hl_close(h);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_tables_sorted);
    RUN_TEST(test_every_dispatch_entry_opens);
    RUN_TEST(test_every_dispatch_entry_lexes);
    return UNITY_END();
}
