# Sprint 06 — Line Range & Highlight-Line

## Goal
Add line selection and emphasis: print only chosen ranges, highlight chosen lines, and cap
squeeze output. Introduces the lookahead machinery that lets the printer know about EOF
without buffering the whole file.

## Targets / Deliverables
- **`range.c`:** `-r/--line-range` in all bat forms — `N:M`, `:M`, `N:`, `-N:` (last N),
  `N:+M` (N plus M more), `N::C` (line N with C context). Multiple `-r` accumulate.
- **Lookahead ring buffer** sized to the largest range offset-from-end, so "last N lines"
  and snip boundaries are known without holding the whole file (mirrors bat's `VecDeque`
  lookahead, audit 02 controller.rs:252).
- **`-H/--highlight-line`** (N:M / :M / N:) — background-emphasis selected lines; multiple
  `-H` accumulate; composes with decorations + snip separators between disjoint ranges.
- **`--squeeze-limit`** — max consecutive blank lines kept under `-s` (default 1).

## Pitfalls (cited)
- **Range grammar off-by-ones** — the six forms have subtle inclusive/relative semantics;
  port bat's parsing precisely and table-test each form against expected line sets.
- **Lookahead sizing** — under-sizing the ring breaks "last N"; over-sizing wastes memory.
  Size to the max offset-from-end across all ranges (audit 02).
- Ranges must filter **before** any expensive rendering so skipped lines cost ~nothing
  (audit 04 — pay only for what's shown).

## Architecture / Perf focus
- Still allocation-free; the ring buffer is a fixed pre-sized scratch.
- Range filtering short-circuits ahead of decoration/highlight work.

## Definition of Done
- Table tests cover all six `-r` forms + multi-range + `-H` accumulation, asserting exact
  emitted line sets.
- Snapshot goldens for `-r` + `--style` + snip separators between disjoint ranges.
- `--squeeze-limit` behaves; parity with `cat -s` at limit 1.
- A "last N lines of a stream" test confirms the lookahead ring works without whole-file
  buffering.
