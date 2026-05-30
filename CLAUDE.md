# CLAUDE.md — working guide for the `mat` repository

`mat` is a from-scratch C11/POSIX reimplementation of `cat` with `bat`'s niceties.
The one goal that overrides the others: **be the fastest of cat/bat/mat**, and
actively *innovate past* both wherever they leave performance on the table — not just
mimic them. Parity with `cat` is the correctness floor; speed is the point.

## The non-negotiable invariant: the TTY/pipe split

`main.c` decides once, up front, which pipeline runs:

- **Fast path** (no transform flags): `main → fastpath → iobuf/output`. This path is
  **allocation-free, stdio-free, locale-free, and loads no assets.** It uses kernel
  zero-copy (`copy_file_range`/`splice`) and falls back to a tight `read`/`write` loop.
- **Cooked / interactive path** (transform flags, or a TTY that wants decorations/
  highlighting): everything expensive lives here, reachable only behind a branch the
  fast path never takes.

A bare `… | mat` or `mat f > f` must never pay for prettiness it isn't using. When you
add a feature, ask: *does this keep the fast path clean?* If it touches malloc/stdio/
locale/asset-loading on the no-flag path, it's in the wrong place.

## Module map (`src/`, one secret per `.c`/`.h`)

| file | responsibility |
|---|---|
| `main.c` | argv → config; the TTY/pipe split; exit-status |
| `cli.c` | hand-rolled argv parsing (no getopt; allocation-free) |
| `config.h` | the `struct config` POD; `xform` bitset gates the cooked path |
| `fastpath.c` | zero-copy ladder: `copy_file_range → splice → read/write` + state machine + `SAME_INODE` guard |
| `iobuf.c` | adaptive buffer sizing + EINTR/partial-safe `full_write` |
| `input.c` | open files/stdin, `-` convention |
| `err.c` | warn vs fatal; sticky exit-status accumulation |
| `compat.h` | OS shims; pulls in `config_generated.h` (configure probes) |

Coming in later sprints: `cooked.c`/`scan*.c`/`expand.c`/`counter.c` (transforms + SIMD),
`decorate.c`/`interactive.c` (gutter/grid), `paging.c`, `range.c`, `highlight/`,
`parallel.c`.

## Build / test / bench

```sh
./configure          # probes copy_file_range/splice/sendfile/fadvise; sets per-OS
                     # FEATURE_CFLAGS; writes config.mk + src/config_generated.h
make                 # builds ./mat — works under GNU make AND BSD make (bmake)
make test            # unit (Unity) + integration goldens + byte-exact cat parity
make bench           # hyperfine vs cat/bat (see Performance below)
make fmt             # clang-format (CI pins clang-format-19 — match it)
make asan            # ASan/UBSan build -> ./mat-asan
```

Run `./configure` before `make`. `config.mk` and `src/config_generated.h` are generated
(gitignored).

## Performance discipline (this is the project)

- **Measure every change** vs `cat` and `bat` with `hyperfine`. If mat isn't faster,
  drill until it is. Claims ship with before/after numbers.
- **Never benchmark on `/dev/zero`.** tmpfs zero-pages flatter any reader (a naive loop
  looked 24× faster than cat on zeros; ~3–4× on `/dev/urandom`, the honest number). Use
  random data. `bench/run.sh` does this.
- Benchmark several shapes: file→/dev/null, file→pipe (`| wc -c`), tiny-file startup.
- **Fewer syscalls is a primary KPI** — check `strace -c` deltas where available.
- The CI perf gate compares against `bench/baseline.json` (enforcing from Sprint 01).

## Testing discipline (tests are first class)

- **Parity is asserted, not hoped for:** `tests/diff/parity.sh` compares mat to the
  system `cat` byte-for-byte (stdout + exit status) across the cat-compat matrix. Extend
  it as flags land.
- **Unit** tests (`tests/unit/test_*.c`, Unity) cover pure functions — scanners, the line
  counter, expansion tables, buffer policy. When SIMD lands, unit tests must prove each
  kernel is bit-identical to the scalar reference on fuzzed input.
- **Integration** goldens (`tests/integration/`) cover output with no `cat` analogue
  (help/version/errors); regenerate with `sh tests/integration/run.sh --update`.
- Keep trunk green: the CI `all-jobs` gate (lint, build matrix, FreeBSD VM, sanitizers)
  is the required check.

## Portability (FreeBSD, Linux, macOS — the user develops on all three)

- C11 + POSIX. Build under both GNU make and bmake — keep the Makefile in the common
  subset (no order-only prereqs, no `$<`/`$^`/pattern rules; `include config.mk` + explicit
  rules + `@mkdir -p`).
- **Feature-test macros matter:** under `-std=c11`, glibc hides POSIX symbols. `configure`
  sets `-D_GNU_SOURCE` (Linux, also exposes `copy_file_range`/`splice`) / `-D_DARWIN_C_SOURCE`
  (macOS). Still `#include` the header that declares what you use (e.g. `<stdlib.h>` for
  `mkstemp`) — clang errors on implicit declarations where gcc only warns.
- Capability is probed, not assumed: guard syscalls with `HAVE_*` from
  `config_generated.h`. FreeBSD has `copy_file_range` but **not** Linux `splice` — the
  FreeBSD CI job exists to catch exactly this kind of divergence.
- SIMD (Sprint 02+) uses runtime CPU dispatch with a scalar fallback; per-ISA kernels are
  compiled with their own `-m` flags but selected at runtime.

## Conventions

- **Commit often, in small logical chunks.** No `git add -A` dumps. Terse, imperative
  subjects; elaborate in the body only when a decision needs it. **Never** add
  `Co-Authored-By` / "Generated with" trailers.
- Note: in some environments gpg signing fails (no secret key) — commit with
  `--no-gpg-sign` there.
- Format with clang-format (config pins the style; CI uses clang-format-19).

## Where the plan lives

Planning, the reference audits (bat + GNU/BSD cat read in full), and the speed-innovation
catalogue live under **`.docs/` — which is gitignored (local only)**. If present, start at
`.docs/sprints/README.md` (sprints 00–09) and `.docs/audits/01–04`. The roadmap is
phased and **cat-first**; the syntax-highlighting engine is deliberately deferred to a late
sprint. Sprint 00 (scaffold + fast cat) is done; Sprint 01 is the zero-copy fast path.
