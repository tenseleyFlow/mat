# Sprint 02 — Cooked Path + SIMD

## Goal
Implement the transformation flags (`-n -b -s -A -v -e -t -E`) the *fast* way:
byte-oriented and buffered like GNU (never wide-char), with SIMD byte-scanning as the
signature optimization that makes `mat` faster than both references on real text. Achieve
full `cat` flag parity.

## Targets / Deliverables
- **`cooked.c` driver:** big input buffer → SIMD-assisted scan for specials → expansion
  via lookup tables + pre-formatted line counter → output buffer → `full_write()`. Output
  buffer over-allocated for expansion (mat's 2× + aggressive flush, vs GNU's 4× — audit
  01 §2).
- **`counter.c`:** GNU-style pre-formatted decimal line counter (`line_buf` + in-place
  increment), 64-bit, `-n`/`-b` (blank suppression), `-s` squeeze (consecutive-newline
  counter **capped at 2** — audit 01 §2). Pure + unit-tested across digit boundaries.
- **`expand.c`:** 256-entry expansion tables for `-v/-t/-e/-A`, each byte → output bytes
  + length. `cat -v` byte semantics: `<32 or 127 → ^X/^?`, `≥128 → M- + low-7-bit
  transform`, tab→`^I` under `-t`, newline→`$` under `-e` (CRLF→`^M$`) (audit 01 §2).
  Table-driven, no branch chains.
- **`scan.c` + kernels:** pure scanners ("next special byte", "count newlines in run")
  with a scalar reference impl and `scan_sse2.c`/`scan_avx2.c`/`scan_neon.c` selected at
  runtime by `dispatch.c` (`__builtin_cpu_supports`; compile-time on NEON). Bulk-`memcpy`
  runs of ordinary bytes, branch only on specials.
- **mmap scan path:** large *regular* inputs in cooked mode use `mmap` +
  `madvise(SEQUENTIAL)` and scan in place; `read()` fallback for non-regular/growing
  files (audit 04 A4).

## Pitfalls (cited)
- **No wide-char.** BSD's `getwc`/`iswprint`/`putwchar` path is slow and its own man page
  admits `-t`/`-v` don't handle multibyte — a red herring. Process bytes, match GNU
  (audit 01 §2).
- **SIMD must be bit-identical to scalar.** `test_scan` fuzzes buffers and asserts every
  available kernel matches the scalar reference exactly; CI runs it on x86 and arm.
- **mmap SIGBUS** if a mapped file is truncated under us — guard with a size check and
  fall back to `read()` for non-regular or growing inputs (audit 04 A4).
- **Squeeze overflow** — cap the newline counter at 2 (audit 01 §2).
- Keep the line counter pre-formatted (no per-line `fprintf` like BSD — audit 01 §2).

## Architecture / Perf focus
- Allocation-free rendering: fixed scratch buffers reused across lines; expansion via
  table lookup, not branches (audit 04 A1, C4).
- 2–4× target on long lines from SIMD; record before/after in `bench/results/02-*.md`.
- The cooked path is still reached only when transform flags are set — piped no-flag
  output stays on the Sprint 01 zero-copy path.

## Definition of Done
- Full `cat` flag parity: `parity.sh` byte-identical to system `cat` for the cross
  product of `{-n,-b,-s,-A,-v,-e,-t,-E,-u}` × fixtures incl. all-256-control-bytes,
  high-bytes, CRLF, no-trailing-newline.
- `test_scan` (kernel↔scalar fuzz), `test_counter`, `test_expand` green on x86 + arm CI.
- mmap path exercised by a large-file fixture; SIGBUS guard covered by a truncation test.
- `bench` shows the SIMD long-line win; numbers committed.
