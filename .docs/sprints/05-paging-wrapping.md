# Sprint 05 — Paging & Wrapping

## Goal
Add interactive paging and width-aware wrapping — the last big pieces of the "feels like
bat on a terminal" experience — while keeping both off the piped fast path and
allocation-frugal.

## Targets / Deliverables
- **`paging.c`:** `--paging {auto,never,always}`, `-P/--no-paging`, `--pager` override.
  Fork/exec `less` with `-R` (ANSI), `-F` (quit if one screen), `-S` (chop), `-K` (quit on
  interrupt), `LESSCHARSET=UTF-8`; sniff `less` version for the old-version `--no-init`
  workaround (audit 02 §output). Write to the pager's stdin pipe.
- **`wrap.c`:** `--wrap {auto,never,character,word}`. Width-aware wrapping that **reuses
  the decoration prefix** across continuation lines instead of regenerating it per wrapped
  line (audit 02 §slow path). Char-width via a compact `wcwidth`-style table.
- Wire `-S/--chop-long-lines` (Sprint 04) to `--wrap=never`.

## Pitfalls (cited)
- **Page only when** output is a TTY **and** there are file inputs **and** paging-mode
  permits (audit 02 controller.rs:57). A bare pipe never spawns a pager.
- **Don't regenerate the wrap prefix per line** — bat's measurable wrapping cost comes from
  rebuilding decoration strings each wrapped line; precompute once and reuse (audit 02,
  audit 04 C4).
- Handle pager death/`SIGPIPE` gracefully (user quits `less` early) — don't crash or leak
  the child.

## Architecture / Perf focus
- Builtin pager (minus-equivalent) is explicitly deferred as a luxury; shell out to `less`
  first (audit 02 §output).
- Wrapping is allocation-free: continuation prefix and line scratch reused (audit 04 C4).
- Neither feature exists on the zero-copy/cooked piped paths.

## Definition of Done
- `mat bigfile` on a TTY pages through `less` with correct flags; quitting early is clean
  (no zombie, no crash) — covered by a PTY-based integration test.
- `--wrap=word`/`character` produce correct continuation lines with the gutter prefix;
  snapshot goldens cover both plus long-line and wide-char fixtures.
- `--paging=never`/pipe path identical to prior sprints (parity).
- `bench`: wrapping shows no per-line allocation regression (track allocations/`heaptrack`
  or an allocation counter in a debug build).
