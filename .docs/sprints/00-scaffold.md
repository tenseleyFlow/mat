# Sprint 00 — Scaffold & Simplest Correct Fast cat

## Goal
Stand up the whole project skeleton — build, test, bench, CI — green on FreeBSD, Linux,
and macOS, plus the dumbest *correct* fast cat. By the end, `mat file...` and
`... | mat` behave exactly like `cat` for the no-flag case, and the machinery that keeps
us honest (tests, parity, benchmarks, CI gate) is in place. Everything after this sprint
slots into a structure that already exists.

## Targets / Deliverables
- **Build system:** portable `Makefile` (written to the BSD+GNU make common subset) +
  a `configure` shell script that writes `config.mk` + `src/config_generated.h` from
  compile-probes. Sprint 00 probes are minimal (compiler, `-std=c11`, pagesize); the
  syscall probes (`HAVE_COPY_FILE_RANGE`, …) land in Sprint 01 but the probe scaffold
  exists now. Targets: `make`, `make test`, `make bench`, `make fmt`, `make tidy`,
  `make asan`, `make clean`, `make install`. Emit `compile_commands.json`.
- **Module skeleton:** every `.h` from the architecture exists with documented
  interfaces; `.c` files stubbed where not yet implemented. `main.c` performs the
  **TTY/pipe split** (`isatty(STDOUT_FILENO)` + transform-flags check) and routes to
  the fast path. Both branches do the same thing today — the split is structural.
- **Fast cat (this sprint's real code):** `iobuf.c` adaptive buffer sizing +
  `full_write()` (retry partial writes + EINTR); a plain `read()`+`full_write()` copy
  loop; `input.c` opening files + stdin/`-`; `err.c` exit-status accumulation. FILE
  args, multiple files concatenated, stdin when no args or `-`.
- **Test harness:** Unity vendored at `tests/vendor/unity/` (`unity.c`, `unity.h`,
  `unity_internals.h`); first unit tests (`test_iobuf` buffer-policy + `full_write`
  partial/EINTR via fake fd; a placeholder `test_smoke`). POSIX-sh golden runner
  `tests/integration/run.sh` (`--update` regen) with 3–4 cases. Differential parity
  `tests/diff/parity.sh` (no-flag case vs system `cat`, byte-exact + exit status).
  Fixtures: empty, single-line, multiline, no-trailing-newline.
- **Benchmark harness:** `bench/run.sh` driving `hyperfine` over scenarios (tiny file,
  large generated file, file→pipe, stdin→pipe) comparing `mat`/`cat`/`bat`; first
  numbers recorded to `bench/results/00-baseline.md`. Gate is informational this sprint.
- **CI:** `.github/workflows/ci.yml` — lint (`clang-format --dry-run -Werror`),
  build+test on `{ubuntu,macos}×{gcc,clang}` with
  `-std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion`, a `freebsd` job via
  `vmactions/freebsd-vm`, and an ASan/UBSan job. `all-jobs` aggregate as the single
  required check. `.clang-format` + a curated `.clang-tidy` checked in.
- **Docs:** top-level `README.md` (what mat is, build, status badge placeholder),
  `LICENSE`, `CONTRIBUTING.md` (commit style, test-first, how to run parity/bench).

## Pitfalls (cited)
- **EINTR / partial writes** — BSD `raw_cat` calls `err(1)` on any short write and has
  no EINTR handling (audit 01 §1). `full_write()` must loop on partial writes and retry
  `EINTR`; treat `EAGAIN` as an error. Read errors are recoverable (log, continue to
  next file); write errors are fatal.
- **stdin lifecycle** — `-` and "no args" both mean stdin; only `close()` stdin if we
  opened it, and multiple `-` re-read stdin (audit 01 §3).
- **No stdio buffering on the fast path** — use raw `read`/`write`, not `FILE*`; the
  hot path stays allocation-free and stdio-free (hot-path invariant).
- **Adaptive buffer** — size from `st_blksize` for non-regular and a memory-aware cap
  (~1MB) for regular files (audit 01 §1); page-align; reuse across files. Don't ship a
  fixed tiny buffer.
- **Make portability** — avoid GNU-make-only constructs (`:=` is fine, but no
  `$(shell)`, no pattern-specific `eval`); push anything fancy into `configure`.

## Architecture / Perf focus
- The **TTY/pipe split** is wired from commit one even though both branches are
  identical today (audit 02 — bat's `loop_through` fork is the load-bearing structural
  idea). This is the seam every later sprint hangs off.
- Establish the **hot-path invariant** and a CI tripwire (stripped binary size budget +
  `mat tiny.txt` startup near `execve`+parse) so we never regress it (audit 04 B3).
- Lazy `setlocale`/`posix_fadvise` deferred to when flags need them (audit 04 B2) —
  Sprint 00 simply doesn't call them.

## Definition of Done
- `./configure && make` builds clean (`-Werror`) on Linux, macOS, FreeBSD.
- `make test` green: Unity units + integration goldens + `parity.sh` byte-identical to
  `cat` for: empty, single-line, multiline, no-trailing-newline, multi-file concat,
  stdin, `-`, and `mat a - b` (file/stdin/file interleave).
- `make bench` produces `bench/results/00-baseline.md` with mat vs cat vs bat numbers.
- CI `all-jobs` green on a PR to trunk across the full matrix incl. the FreeBSD VM and
  the sanitizer job.
- Committed in small chunks (build scaffold → io/err → tests → bench → CI → docs), terse
  imperative messages.
- **After green:** create `tenseleyFlow/mat` via `gh` and push (the plan's step 3).
