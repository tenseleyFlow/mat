# mat — Sprint Plan

The development scaffolding for `mat`: a from-scratch C11/POSIX reimplementation of
`cat` with `bat`'s niceties, whose north star is being **the fastest of cat/bat/mat**
and *innovating past* both wherever they leave performance on the table (see
[`overview.md`](../overview.md) and [`audits/04-innovation-and-speed.md`](../audits/04-innovation-and-speed.md)).

Each sprint doc has a fixed shape: **Goal · Targets · Pitfalls (audit-cited) ·
Architecture / Perf focus · Definition of Done**. Work proceeds 00 → 09. We commit
often in small chunks; trunk stays green on every push (CI gate from Sprint 00).

## Sprints
| # | Title | Theme |
|---|-------|-------|
| [00](00-scaffold.md) | Scaffold & simplest correct fast cat | Project skeleton, build/test/bench/CI, the TTY/pipe split, a correct fast cat |
| [01](01-zero-copy-fastpath.md) | Zero-copy fast path | `copy_file_range → splice → read/write` ladder, syscall-class state machine |
| [02](02-cooked-path-simd.md) | Cooked path + SIMD | `-n -b -s -A -v -e -t -E`, SIMD scanning, mmap scan |
| [03](03-decorations.md) | Decoration model (no highlight) | Gutter/panel/grid vtable, `--style`, `--color`, TTY-only prettiness |
| [04](04-config-cli.md) | Config, env precedence, CLI completeness | Config files, `BAT_*` env, help/version, man draft |
| [05](05-paging-wrapping.md) | Paging & wrapping | `--paging/--pager`, `less -RFS`, `--wrap` |
| [06](06-line-range.md) | Line range & highlight-line | `-r` (all forms), `-H`, lookahead ring |
| [07](07-encodings-mapping.md) | Encodings & file mapping | UTF-8-first sniff, UTF-16/binary, `-m`, `--strip-ansi` |
| [08](08-highlighting.md) | Syntax highlighting (engine decided here) | Lazy assets, detection order, the deferred hard frontier |
| [09](09-parallel-git-release.md) | Parallel pipeline + git + release | Reader/transform/writer threads, `vmsplice`, git markers, release artifacts |

## Speed-innovation → sprint mapping (from audit 04's ROI ladder)
1. **TTY/pipe split + zero-copy + adaptive buffers** (most raw-speed win, low risk) → **00–01**
2. **Lazy startup / locale / fadvise / tiny binary** (decisively beats bat's common case) → **00–04, enforced ongoing**
3. **SIMD cooked-path scanning + allocation-free rendering** → **02–03**
4. **mmap scan + UTF-8-first sniff** → **02, 07**
5. **Cheap highlighting** (hard frontier, speed-led engine choice) → **08**
6. **Parallel pipeline** (highest ceiling, highest complexity, last) → **09**

## Conventions
- **Hot-path invariant:** `main → fastpath → iobuf/output` touches no `malloc`, no
  stdio, no locale, no asset load. Anything expensive sits behind a branch the fast
  path never takes. Enforced by review + a CI size/startup tripwire.
- **Prove it:** every optimization ships with a before/after `hyperfine` number and,
  where relevant, an `strace -c` syscall delta. "Blazingly fast" is measured, not vibes.
- **Parity is a test, not a hope:** `tests/diff/parity.sh` asserts byte-identical
  output + exit status vs the system `cat` across the cat-compat matrix.
- References live (untracked) under [`.docs/refs/`](../refs/); audits under
  [`.docs/audits/`](../audits/).
