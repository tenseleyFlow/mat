#include "unity.h"
#include "scan.h"

#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

#define BUFN 8192
static unsigned char buf[BUFN];

/* Deterministic LCG so the corpus is identical everywhere. */
static void fill(unsigned seed)
{
    unsigned x = seed * 2654435761u + 1u;
    for (int i = 0; i < BUFN; i++) {
        x = x * 1103515245u + 12345u;
        buf[i] = (unsigned char)(x >> 17);
    }
}

/* Compare every available SIMD kernel against the scalar reference. */
static void compare_kernels(const unsigned char *p, const unsigned char *end)
{
    const unsigned char *nl = mat_scan_newline_scalar(p, end);
    const unsigned char *np = mat_scan_nonprint_scalar(p, end);
#if defined(__x86_64__) || defined(__i386__)
    if (__builtin_cpu_supports("sse2")) {
        TEST_ASSERT_EQUAL_PTR(nl, mat_scan_newline_sse2(p, end));
        TEST_ASSERT_EQUAL_PTR(np, mat_scan_nonprint_sse2(p, end));
    }
    if (__builtin_cpu_supports("avx2")) {
        TEST_ASSERT_EQUAL_PTR(nl, mat_scan_newline_avx2(p, end));
        TEST_ASSERT_EQUAL_PTR(np, mat_scan_nonprint_avx2(p, end));
    }
#elif defined(__aarch64__)
    TEST_ASSERT_EQUAL_PTR(nl, mat_scan_newline_neon(p, end));
    TEST_ASSERT_EQUAL_PTR(np, mat_scan_nonprint_neon(p, end));
#else
    (void)nl;
    (void)np;
#endif
}

/* Random corpus across many start offsets and lengths — this exercises every
 * vector/scalar boundary and tail length. */
static void test_fuzz(void)
{
    for (unsigned seed = 1; seed <= 120; seed++) {
        fill(seed);
        for (int start = 0; start < 40; start++) {
            for (int len = 0; len <= 80 && start + len <= BUFN; len++)
                compare_kernels(buf + start, buf + start + len);
            compare_kernels(buf + start, buf + BUFN); /* a long window */
        }
    }
}

/* Degenerate and boundary-aligned inputs. */
static void test_corners(void)
{
    compare_kernels(buf, buf); /* empty */

    memset(buf, '\n', 200);
    compare_kernels(buf, buf + 200); /* all matches */
    memset(buf, 'A', 200);
    compare_kernels(buf, buf + 200); /* no newline; all printable */
    memset(buf, 0xFF, 200);
    compare_kernels(buf, buf + 200); /* all nonprint, no newline */

    /* a single special byte at each position around the 16/32 boundaries */
    for (int pos = 0; pos < 70; pos++) {
        memset(buf, 'x', 128);
        buf[pos] = '\n';
        compare_kernels(buf, buf + 128);
        memset(buf, 'x', 128);
        buf[pos] = 0x01; /* control: nonprint but not newline */
        compare_kernels(buf, buf + 128);
        memset(buf, 'x', 128);
        buf[pos] = 0x80; /* high byte */
        compare_kernels(buf, buf + 128);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fuzz);
    RUN_TEST(test_corners);
    return UNITY_END();
}
