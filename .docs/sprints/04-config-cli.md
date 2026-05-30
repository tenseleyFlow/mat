# Sprint 04 — Config, Env Precedence, CLI Completeness

## Goal
Round out the command-line surface short of highlighting and add layered configuration —
without ever letting config work creep onto the fast path. After this sprint `mat` has a
complete, well-behaved CLI and respects user config the way `bat` does.

## Targets / Deliverables
- **`cli.c` completeness:** `--tabs`, `--terminal-width`, `-p/--plain` (count form `-pp`),
  `-S/--chop-long-lines`, `-E/--quiet-empty`, `--italic-text`, plus `--help`/`-h` (short +
  long) and `--version`/`-V`. Hand-rolled parser, allocation-free for the common path.
- **Config + env precedence** (audit 03): defaults < `/etc/mat/config` (overridable via a
  system-config-prefix env, for testability) < `~/.config/mat/config` < `MAT_OPTS` <
  individual env (`MAT_THEME`, `MAT_STYLE`, `MAT_PAGER`, `MAT_PAGING`, `MAT_TABS`,
  `MAT_WIDTH`) < CLI. Also accept `BAT_*` as fallbacks for muscle-memory compat (documented).
  Honor `NO_COLOR`. `--no-config` to skip it all.
- **Config introspection:** `--config-file`, `--config-dir`, `--generate-config-file`.
- **Man page draft** (`man/mat.1`) and **completions skeleton** (bash/fish/zsh) generated
  from a single option table so they can't drift from `cli.c`.

## Pitfalls (cited)
- **Config must not run before the fast-path branch when piped.** Reading
  `~/.config/mat/config` is filesystem work; for a bare `… | mat` with no relevant flags,
  we must reach the zero-copy path with startup still near `execve`+parse (audit 04 B1/B3).
  Resolve config lazily and only when a flag/TTY actually needs it.
- **Precedence order** is load-bearing for compatibility; mirror bat's exactly and test it.
- Keep the parser allocation-free on the hot path (audit 04 B3).

## Architecture / Perf focus
- Lazy config: the fast path never touches config files. A startup-time CI tripwire guards
  against regressions here (audit 04 B3).
- Single source of truth for options → `cli.c`, man page, completions all generated/checked
  from one table (prevents drift; audit 03).

## Definition of Done
- Every cat-compat + non-highlight bat flag parses and behaves; `--help`/`--version`
  stable (snapshot-tested).
- A config-precedence test asserts the full chain (fixture config dirs + env + CLI) resolves
  correctly, mirroring bat's order.
- Startup tripwire: `… | mat` (no relevant flags) startup unchanged vs Sprint 02 despite a
  populated `~/.config/mat/config` present.
- Man page renders (`man ./man/mat.1`) and completions load in their shells (smoke-tested).
