#include "scan.h"

/*
 * Scalar reference. "nonprint" tests membership outside [32,126] with one
 * unsigned comparison: (byte - 32) wraps so that 0..31 land at 224..255 and
 * 127..255 land at 95..223 — everything special is > 94.
 *
 * The SSE2/AVX2 kernels below use per-function target attributes so this single
 * file compiles with baseline flags; __builtin_cpu_supports() guarantees a
 * kernel only runs on a CPU that has it. NEON is baseline on aarch64. Every
 * kernel is proven identical to the scalar version by tests/unit/test_scan.c.
 */
const unsigned char *mat_scan_newline_scalar(const unsigned char *p,
                                             const unsigned char *end)
{
    for (; p < end; p++)
        if (*p == '\n')
            return p;
    return end;
}

const unsigned char *mat_scan_nonprint_scalar(const unsigned char *p,
                                              const unsigned char *end)
{
    for (; p < end; p++)
        if ((unsigned char)(*p - 32) > 94)
            return p;
    return end;
}

mat_scan_fn mat_scan_newline = mat_scan_newline_scalar;
mat_scan_fn mat_scan_nonprint = mat_scan_nonprint_scalar;

/* ---------------- x86: SSE2 + AVX2 ---------------- */
#if defined(__x86_64__) || defined(__i386__)
#define MAT_X86 1
#include <immintrin.h>

__attribute__((target("sse2"))) const unsigned char *
mat_scan_newline_sse2(const unsigned char *p, const unsigned char *end)
{
    const __m128i nl = _mm_set1_epi8('\n');
    for (; p + 16 <= end; p += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)p);
        unsigned m = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(v, nl));
        if (m)
            return p + __builtin_ctz(m);
    }
    return mat_scan_newline_scalar(p, end);
}

__attribute__((target("sse2"))) const unsigned char *
mat_scan_nonprint_sse2(const unsigned char *p, const unsigned char *end)
{
    const __m128i bias = _mm_set1_epi8(32);
    const __m128i thr = _mm_set1_epi8(94);
    const __m128i zero = _mm_setzero_si128();
    for (; p + 16 <= end; p += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)p);
        __m128i s = _mm_subs_epu8(_mm_sub_epi8(v, bias), thr);
        unsigned plain = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(s, zero));
        unsigned special = (~plain) & 0xFFFFu;
        if (special)
            return p + __builtin_ctz(special);
    }
    return mat_scan_nonprint_scalar(p, end);
}

__attribute__((target("avx2"))) const unsigned char *
mat_scan_newline_avx2(const unsigned char *p, const unsigned char *end)
{
    const __m256i nl = _mm256_set1_epi8('\n');
    for (; p + 32 <= end; p += 32) {
        __m256i v = _mm256_loadu_si256((const __m256i *)p);
        unsigned m = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, nl));
        if (m)
            return p + __builtin_ctz(m);
    }
    return mat_scan_newline_scalar(p, end);
}

__attribute__((target("avx2"))) const unsigned char *
mat_scan_nonprint_avx2(const unsigned char *p, const unsigned char *end)
{
    const __m256i bias = _mm256_set1_epi8(32);
    const __m256i thr = _mm256_set1_epi8(94);
    const __m256i zero = _mm256_setzero_si256();
    for (; p + 32 <= end; p += 32) {
        __m256i v = _mm256_loadu_si256((const __m256i *)p);
        __m256i s = _mm256_subs_epu8(_mm256_sub_epi8(v, bias), thr);
        unsigned plain =
            (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(s, zero));
        if (plain != 0xFFFFFFFFu)
            return p + __builtin_ctz(~plain);
    }
    return mat_scan_nonprint_scalar(p, end);
}
#endif /* x86 */

/* ---------------- aarch64: NEON ---------------- */
#if defined(__aarch64__)
#define MAT_NEON 1
#include <arm_neon.h>

/* Narrow 16 match bytes to a bitmask via vshrn + vget_lane, then ctz to find
 * the first set bit. Avoids the scalar byte-scan on a match. */
static inline int neon_first_set(uint8x16_t mask)
{
    uint8x8_t narrow = vshrn_n_u16(vreinterpretq_u16_u8(mask), 4);
    uint64_t bits = vget_lane_u64(vreinterpret_u64_u8(narrow), 0);
    if (bits == 0)
        return -1;
    return __builtin_ctzll(bits) / 4;
}

const unsigned char *mat_scan_newline_neon(const unsigned char *p,
                                           const unsigned char *end)
{
    const uint8x16_t nl = vdupq_n_u8('\n');
    for (; p + 16 <= end; p += 16) {
        uint8x16_t eq = vceqq_u8(vld1q_u8(p), nl);
        if (vmaxvq_u8(eq)) {
            int idx = neon_first_set(eq);
            if (idx >= 0)
                return p + idx;
        }
    }
    return mat_scan_newline_scalar(p, end);
}

const unsigned char *mat_scan_nonprint_neon(const unsigned char *p,
                                            const unsigned char *end)
{
    const uint8x16_t bias = vdupq_n_u8(32);
    const uint8x16_t thr = vdupq_n_u8(94);
    for (; p + 16 <= end; p += 16) {
        uint8x16_t s = vqsubq_u8(vsubq_u8(vld1q_u8(p), bias), thr);
        if (vmaxvq_u8(s)) {
            /* s is nonzero at nonprint positions; treat as a match mask */
            int idx = neon_first_set(s);
            if (idx >= 0)
                return p + idx;
        }
    }
    return mat_scan_nonprint_scalar(p, end);
}
#endif /* aarch64 */

void mat_scan_init(void)
{
#if defined(MAT_X86)
    if (__builtin_cpu_supports("avx2")) {
        mat_scan_newline = mat_scan_newline_avx2;
        mat_scan_nonprint = mat_scan_nonprint_avx2;
    } else if (__builtin_cpu_supports("sse2")) {
        mat_scan_newline = mat_scan_newline_sse2;
        mat_scan_nonprint = mat_scan_nonprint_sse2;
    }
#elif defined(MAT_NEON)
    mat_scan_newline = mat_scan_newline_neon;
    mat_scan_nonprint = mat_scan_nonprint_neon;
#endif
}
