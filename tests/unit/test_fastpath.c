#include "unity.h"
#include "fastpath.h"

#include <errno.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* copy_file_range: regular -> regular only, and only when enabled. */
static void test_cfr_eligible(void)
{
    TEST_ASSERT_TRUE(mat_cfr_eligible(true, true, true));    /* reg -> reg */
    TEST_ASSERT_FALSE(mat_cfr_eligible(true, true, false));  /* disabled */
    TEST_ASSERT_FALSE(mat_cfr_eligible(false, true, true));  /* pipe -> reg */
    TEST_ASSERT_FALSE(mat_cfr_eligible(true, false, true));  /* reg -> pipe */
    TEST_ASSERT_FALSE(mat_cfr_eligible(false, false, true)); /* pipe -> pipe */
}

/* splice: anything -> pipe, but skip tiny regular files and respect the flag.
 */
static void test_splice_eligible(void)
{
    const off_t big = 1 << 20;
    const off_t tiny = 1024;
    const off_t threshold = 32768;

    /* large regular file -> pipe: yes */
    TEST_ASSERT_TRUE(mat_splice_eligible(true, big, true, true));
    /* pipe input -> pipe (non-regular has no meaningful size): yes */
    TEST_ASSERT_TRUE(mat_splice_eligible(false, 0, true, true));
    /* tiny regular file -> pipe: no (read/write is faster) */
    TEST_ASSERT_FALSE(mat_splice_eligible(true, tiny, true, true));
    /* exactly at the threshold counts as small: no */
    TEST_ASSERT_FALSE(mat_splice_eligible(true, threshold, true, true));
    /* one byte over the threshold: yes */
    TEST_ASSERT_TRUE(mat_splice_eligible(true, threshold + 1, true, true));
    /* output is not a pipe: no */
    TEST_ASSERT_FALSE(mat_splice_eligible(true, big, false, true));
    /* splice disabled: no */
    TEST_ASSERT_FALSE(mat_splice_eligible(true, big, true, false));
}

/* The copy_file_range fallback-errno set: these mean "unsupported, try next".
 */
static void test_cfr_fallback_errno(void)
{
    TEST_ASSERT_TRUE(mat_cfr_fallback_errno(ENOSYS));
    TEST_ASSERT_TRUE(mat_cfr_fallback_errno(EINVAL));
    TEST_ASSERT_TRUE(mat_cfr_fallback_errno(EBADF));
    TEST_ASSERT_TRUE(mat_cfr_fallback_errno(EXDEV));
    TEST_ASSERT_TRUE(mat_cfr_fallback_errno(ETXTBSY));
    TEST_ASSERT_TRUE(mat_cfr_fallback_errno(EPERM));
    TEST_ASSERT_TRUE(mat_cfr_fallback_errno(EFBIG));

    /* Genuine I/O errors are NOT fallbacks — they must surface. */
    TEST_ASSERT_FALSE(mat_cfr_fallback_errno(EIO));
    TEST_ASSERT_FALSE(mat_cfr_fallback_errno(ENOSPC));
    TEST_ASSERT_FALSE(mat_cfr_fallback_errno(EACCES));
    TEST_ASSERT_FALSE(mat_cfr_fallback_errno(0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cfr_eligible);
    RUN_TEST(test_splice_eligible);
    RUN_TEST(test_cfr_fallback_errno);
    return UNITY_END();
}
