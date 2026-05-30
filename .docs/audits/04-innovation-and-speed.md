# Audit 04 — Innovation & Speed: How `mat` Beats cat *and* bat

> North star: **SPEEEEEED.** mat should be the fastest of the three on every axis —
> raw throughput, startup latency, and "pretty output" cost. cat and bat are not
> perfect; this doc enumerates concrete places they leave performance on the table and
> what mat does differently. These are the headline differentiators, not just parity.

Each item: **the gap → mat's move → expected win → risk**. Items are tagged with the
sprint where they'd land so the speed thesis is threaded through the whole plan, not
bolted on at the end.

---

## A. Raw throughput (the cat-mode fast path)

### A1. SIMD byte scanning — *the* signature optimization
- **Gap**: GNU scans byte-by-byte in the cooked path (`*bpin++` loop); BSD uses
  `getc()` (worse). Neither vectorizes newline/special-byte detection.
- **mat**: SSE2/AVX2 (and NEON on Apple Silicon/arm) to find newlines and special
  bytes 16–32 bytes per instruction; bulk-`memcpy` runs of ordinary bytes and only
  branch on specials. Runtime CPU dispatch (`__builtin_cpu_supports`) with a scalar
  fallback for portability.
- **Win**: 2–4× on line-numbering / squeeze / `-v` over long lines; faster `wc`-style
  newline counting for `-n`.
- **Risk**: per-arch code + dispatch complexity; must keep a correct scalar path.
- **Sprint**: scaffolded in core, lands ~02 (cooked path).

### A2. Adaptive large buffers
- **Gap**: GNU uses fixed `io_blksize` (often 128KB or less); BSD adapts but caps
  oddly.
- **mat**: size buffer from `st_blksize`, physical memory, and file size; cap ~1MB.
  Page-aligned, reused across files, freed if huge to bound peak RSS.
- **Win**: fewer syscalls on big files; measurable on multi-GB inputs.
- **Sprint**: 00–01.

### A3. Zero-copy decision tree done right
- **Gap**: BSD only does `copy_file_range`; GNU does cfr+splice but no `sendfile`;
  neither uses `vmsplice` for cooked→pipe.
- **mat**: `copy_file_range → splice → sendfile(reg→socket) → read/full_write`, with a
  GNU-style state machine that never re-attempts a syscall class that already failed.
  `vmsplice` for cooked output into a pipe (skip the final copy).
- **Win**: near-disk-bandwidth file→file; avoids a userspace bounce on pipe targets.
- **Sprint**: 01 (fast path), vmsplice as a later optimization.

### A4. mmap scan path for large regular files in cooked mode
- **Gap**: both `read()` into a buffer even when the kernel could map the file.
- **mat**: for large regular-file inputs in cooked mode, `mmap` + `madvise(SEQUENTIAL)`
  and scan in place — no read buffer, no copy until output.
- **Win**: removes one full copy of the file through userspace.
- **Risk**: mmap of files that change/truncate under us (SIGBUS) — guard with size
  checks and fall back to read() for non-regular or growing files.
- **Sprint**: 02, behind a fallback.

---

## B. Startup latency (where bat is embarrassingly beatable)

### B1. No multi-hundred-KB asset deserialization on the hot path
- **Gap**: bat deserializes ~800KB of syntax data via `OnceCell` even for tiny files;
  dominant cost for `bat one-line.txt`.
- **mat**: cat-mode and decoration-only modes load **nothing** beyond argv parsing.
  Highlighting assets load lazily *only* when a TTY actually needs highlighting, and
  even then we prefer a compact format with near-zero parse cost (e.g. `mmap` a
  prebuilt table rather than decompress+deserialize).
- **Win**: startup in the cat-mode common case is essentially `execve` + parse argv —
  comparable to cat, far below bat.
- **Sprint**: structural from 00; highlighting-asset format decided at the highlight
  sprint.

### B2. Lazy locale + lazy fadvise
- **Gap**: both `setlocale(LC_CTYPE,"")` unconditionally; GNU `posix_fadvise` per file.
- **mat**: `setlocale` only when `-v` needs it; `fadvise` only for large files.
- **Win**: shaves syscalls/CRT init off every invocation.
- **Sprint**: 00–01.

### B3. Tiny static binary, hot path branch-free
- **mat**: aim for a small statically-linkable binary (musl target), no dynamic
  highlighting deps unless used; mark the fast path with `__builtin_expect` hints and
  keep it allocation-free.
- **Sprint**: ongoing; enforced by a binary-size + startup-time CI check.

---

## C. "Pretty output" cost (beating bat at its own game)

### C1. Pay for prettiness only when asked
- **Gap**: bat runs the full highlight+decorate pipeline even when piped unless it
  detects `loop_through`.
- **mat**: hard TTY/pipe split — piped/redirected output is *always* the cat-fast path
  unless the user forces color. Decorations without highlighting (numbers/grid/header)
  are cheap and don't touch any syntax engine.
- **Sprint**: 02–03.

### C2. UTF-8-first input sniffing (skip bat's two-pass)
- **Gap**: bat eagerly reads + re-reads the first line for UTF-16/BOM handling.
- **mat**: sniff one leading block; UTF-8 is the zero-overhead happy path; UTF-16/
  binary are explicit slow branches.
- **Sprint**: 02 / 07.

### C3. Cheaper highlighting strategy (engine choice deferred, but speed-led)
- **Gap**: syntect is a regex/TextMate FSM — correct but heavy; the 16KB long-line
  cutoff exists *because* it can blow up.
- **mat options** (decide at the highlight sprint, ranked by speed): (a) hand-written
  lexers for the top ~10 languages (fastest, most work), (b) a compact precompiled
  grammar table mmap'd at use, (c) port/embed a TextMate engine for breadth. Keep the
  long-line guard regardless. Highlight in parallel across lines/files where safe.
- **Sprint**: 05.

### C4. Allocation-free line rendering
- **Gap**: bat allocates per line (Vec/String) and regenerates wrap-prefix strings per
  wrapped line.
- **mat**: fixed scratch buffers reused across lines; decoration text precomputed once;
  styled segments are pointers into scratch, not new allocations.
- **Sprint**: 02–03.

---

## D. Parallelism (an axis neither cat nor bat exploits)
- **Gap**: both are single-threaded.
- **mat**: optional pipeline parallelism — reader thread fills buffers while a
  transform thread scans/encodes and a writer thread drains; for many-file invocations,
  highlight files concurrently and serialize only the final ordered write. Off by
  default for tiny inputs (thread spawn isn't free); enabled past a size threshold.
- **Win**: overlaps IO wait with CPU transform; big on slow disks / network FS.
- **Risk**: ordering, error propagation, TSan coverage in CI.
- **Sprint**: optimization sprint after parity; gated behind a threshold + flag.

---

## E. Measurement discipline (so "blazingly fast" is provable, not vibes)
- **Benchmark harness from Sprint 00**: `hyperfine` comparing `mat` vs `cat` vs `bat`
  across fixtures (tiny file, huge file, many small files, long-line file, file→file,
  file→pipe, stdin→pipe), recorded to a tracked results file.
- **CI perf gate**: fail the build if mat regresses beyond a threshold vs its own
  baseline; assert mat ≥ cat on the fast path and ≫ bat on startup.
- **`perf`/`cachegrind` profiles** captured for the hot path; track syscall counts
  (`strace -c`) — fewer syscalls is a primary KPI.
- This doc's claims are hypotheses until the harness proves them; every optimization
  ships with a before/after number.

---

## Priority ladder (speed ROI, highest first)
1. TTY/pipe split + zero-copy fast path (A3) + adaptive buffers (A2) — *most* of the
   raw-speed win, low risk. **Sprint 00–01.**
2. Lazy everything at startup (B1–B3) — beats bat decisively on the common case.
3. SIMD cooked-path scanning (A1) + allocation-free rendering (C4).
4. mmap scan (A4), UTF-8-first sniff (C2).
5. Parallel pipeline (D) — biggest ceiling, highest complexity, last.
6. Cheap highlighting (C3) — the hard frontier; speed-led engine choice.
