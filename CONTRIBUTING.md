# Contributing to mat

## Principles
- **Senior-level C.** Clear ownership of memory and errors; one secret per
  translation unit; the hot path stays allocation-free, stdio-free, locale-free.
- **Tests are first class.** Every behavior change ships with a test. Parity
  with `cat` is asserted, not assumed.
- **Prove the speed.** Performance claims come with a before/after benchmark.

## Workflow
```sh
./configure
make            # GNU make or BSD make; no external deps
make test       # unit + integration + parity — must be green before you commit
make fmt        # clang-format (CI pins clang-format-19; match it)
make asan       # ASan/UBSan build -> ./mat-asan
make bench      # vs cat/bat (hyperfine optional)
```

## Tests
- **Unit** (`tests/unit/test_*.c`, Unity): pure functions — scanners, the line
  counter, expansion tables, buffer policy.
- **Integration** (`tests/integration/`): golden stdout/stderr/exit for output
  with no `cat` analogue (help, version, errors). Regenerate with
  `sh tests/integration/run.sh --update`.
- **Parity** (`tests/diff/parity.sh`): byte-identical to the system `cat` across
  the cat-compat matrix. Add cases as flags land.

## Commits
- Commit often, in small chunks. No `git add -A` dumps.
- Terse, imperative subject lines (< ~50 chars); elaborate in the body only when
  a decision needs it.
- Keep trunk green: the `all-jobs` CI gate (lint, build matrix, FreeBSD VM,
  sanitizers) must pass.

## Layout
- `src/` — implementation, one concern per `.c`/`.h`.
- `tests/` — `unit/`, `integration/`, `diff/`, vendored `vendor/unity/`.
- `bench/` — benchmark harness + recorded results.
- `.docs/` — planning, sprint plan, reference audits (local only, untracked).
  Start at `.docs/sprints/README.md`.
