# Sprint 03 — Decoration Model (no highlight)

## Goal
Build the fancy printer scaffold — line-number gutter, grid, header, rule, snip — that
pays for prettiness *only* on a TTY and touches no syntax engine. This hardens the
TTY/pipe split into a real visible difference and ports bat's decoration/panel model into
C.

## Targets / Deliverables
- **`term.c`:** terminal width via `ioctl(TIOCGWINSZ)` → `$COLUMNS` → `--terminal-width`
  (absolute or `±offset`); `isatty`; honor `NO_COLOR`.
- **`style.c`:** parse `--style` comma list with `+`/`-` modifiers into a decoration
  bitset (`numbers, grid, header, header-filename, header-filesize, rule, snip`); `--color
  {auto,never,always}`, `--decorations {auto,never,always}`, `-p/--plain`,
  `-f/--force-colorization`.
- **`decorate.c`:** gutter as a small array of decoration descriptors, each a tiny C
  vtable `{ size_t width; void render(line_no, bool continuation, char *scratch) }`.
  `panel_width = count + Σ width` computed **once**; grid border (`│`, with `─`/`┬┼┴`
  rules) pushed **after** the width calc; whole panel disabled if `term_width < panel + 5`
  (audit 02 §decorations).
- **`interactive.c`:** the fancy printer — header/grid/numbers/rule/snip over cooked
  output; reuses `cooked.c`/`expand.c` for nonprintable glyphs and adds bat-style Unicode
  nonprintable notation (`·`, `␊`, …) as an option alongside `cat -v` bytes
  (`--nonprintable-notation {unicode,caret}`, audit 02 §nonprintable).
- ANSI color emission helper (`as_terminal_escaped` analogue) — minimal, no theme engine
  yet (theming arrives with highlighting in Sprint 08).

## Pitfalls (cited)
- **Grid-after-width** ordering: computing panel width before adding the grid border
  avoids the circular dependency bat carefully sidesteps (audit 02 §decorations).
- **Piped output stays fast.** Decorations engage only when `--decorations`/`--color`
  resolve to a TTY-on state (or are forced); a bare pipe still falls to the Sprint 01/02
  paths (audit 04 C1).
- **Precompute decoration text once**, not per line; line-number width grows past 10000 —
  handle the continuation-line width bump (audit 02 §decorations, audit 04 C4).

## Architecture / Perf focus
- Decorations are allocation-free: render into reused scratch; no per-line `String`
  churn like bat (audit 04 C4).
- No syntax engine on this path — decoration-only output must stay cheap and is a
  separate, fast pipeline from highlighting (which is Sprint 08).

## Definition of Done
- `mat --style=numbers,grid file` on a TTY renders a correct gutter/grid; snapshot tests
  (`run.sh --update` goldens) cover each `--style` combination and `+/-` modifiers.
- Differential check vs `bat --style=...` (BAT_* cleared) matches where semantics are
  intended to align; intentional divergences (caret vs Unicode glyphs) recorded in the
  parity allowlist (audit 03 testing).
- Piped output (no TTY, no force) is byte-identical to the Sprint 02 result — proven by
  parity tests; decoration code never runs.
- `bench`: decoration-on TTY render shows no regression to the piped fast path.
