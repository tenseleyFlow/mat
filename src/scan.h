/*
 * scan.h — byte scanners: the one place SIMD plugs into the cooked path.
 *
 * Each scanner finds the next "interesting" byte so the driver can bulk-copy
 * the plain run between specials instead of branching per byte. The function
 * pointers default to portable scalar implementations and are upgraded to
 * SSE2/AVX2 (x86) or NEON (arm) by mat_scan_init() at startup. Every SIMD
 * kernel is proven bit-identical to the scalar reference by the fuzz test.
 */
#ifndef MAT_SCAN_H
#define MAT_SCAN_H

typedef const unsigned char *(*mat_scan_fn)(const unsigned char *p,
                                            const unsigned char *end);

/* First byte equal to '\n' in [p, end), or end if none. */
extern mat_scan_fn mat_scan_newline;

/* First byte outside the printable range [32, 126] in [p, end), or end. */
extern mat_scan_fn mat_scan_nonprint;

/* Select the best kernels for this CPU. Idempotent; call once at startup. */
void mat_scan_init(void);

/* Scalar reference implementations — exposed for the fuzz test. */
const unsigned char *mat_scan_newline_scalar(const unsigned char *p,
                                             const unsigned char *end);
const unsigned char *mat_scan_nonprint_scalar(const unsigned char *p,
                                              const unsigned char *end);

/* SIMD kernels, exposed so the fuzz test can check each against the scalar
 * reference. Guard each call with the matching __builtin_cpu_supports(). */
#if defined(__x86_64__) || defined(__i386__)
const unsigned char *mat_scan_newline_sse2(const unsigned char *p,
                                           const unsigned char *end);
const unsigned char *mat_scan_nonprint_sse2(const unsigned char *p,
                                            const unsigned char *end);
const unsigned char *mat_scan_newline_avx2(const unsigned char *p,
                                           const unsigned char *end);
const unsigned char *mat_scan_nonprint_avx2(const unsigned char *p,
                                            const unsigned char *end);
#endif
#if defined(__aarch64__)
const unsigned char *mat_scan_newline_neon(const unsigned char *p,
                                           const unsigned char *end);
const unsigned char *mat_scan_nonprint_neon(const unsigned char *p,
                                            const unsigned char *end);
#endif

#endif /* MAT_SCAN_H */
