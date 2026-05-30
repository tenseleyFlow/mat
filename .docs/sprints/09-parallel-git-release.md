# Sprint 09 — Parallel Pipeline + Git + Release

## Goal
Land the highest-ceiling optimization (an axis neither cat nor bat exploits — pipeline
parallelism), add the remaining bat feature (git change markers), and ship: release
artifacts, completions, diagnostics. The 1.0 sprint.

## Targets / Deliverables
- **`parallel.c`:** reader → transform → writer pipeline so IO wait overlaps CPU transform;
  for many-file invocations, highlight files concurrently and serialize only the final
  **ordered** write. **Threshold-gated** + opt-out flag — off for tiny inputs where thread
  spawn isn't worth it; below threshold everything degrades to the single-threaded
  allocation-free fast path (audit 04 D).
- **`vmsplice`** for cooked→pipe output, skipping the final userspace copy (audit 04 A3/C).
- **`git.c`:** `-d/--diff`, `--diff-context`, and the `changes` style marker (`+ ~ ‾ _`).
  Feature-gated and **lazy** — the per-file `git diff` subprocess is bat's cost; keep it
  optional and only when the `changes`/`-d` path is active (audit 02 §perf, audit 03).
- **CI:** add the **TSan** job now that threads exist (audit 04 D).
- **`release.yml`:** on tag, build per-platform `tar.gz` (Linux gnu + musl-static, macOS
  x86_64 + arm64, FreeBSD), `.deb`, man page, completions; attach to a GitHub Release
  (mirrors bat minus Windows, audit 03 CI).
- Finalize `--diagnostic`, cache subcommand (if kept), `--acknowledgements`,
  `--set-terminal-title`.

## Pitfalls (cited)
- **Ordering + error propagation across threads** is the core risk — TSan coverage in CI is
  mandatory; the ordered-write stage must preserve input order exactly (audit 04 D).
- **Thread spawn isn't free** — keep the size/file-count threshold; tiny inputs must stay on
  the single-threaded path (audit 04 D).
- **Git subprocess is slow** — never spawn `git diff` unless change markers/`-d` are
  actually requested (audit 02 §perf).

## Architecture / Perf focus
- Biggest wins on slow/network filesystems where IO wait dominates; measure on a throttled
  mount.
- Everything still falls back to the proven allocation-free single-threaded fast path below
  threshold — parallelism is additive, never a tax on the common case.

## Definition of Done
- Parallel pipeline shows throughput gains on large/multi-file/slow-FS scenarios in
  `bench/results/09-*.md`, with output **byte-identical** to single-threaded (parity +
  ordered-write test); TSan job clean.
- `-d/--diff` + `changes` markers match bat on a git fixture; no `git` subprocess spawned
  when markers aren't requested (verified via `strace`/process trace).
- `release.yml` produces all artifacts on a tag; a test tag yields a complete draft release.
- Full suite (unit + integration + parity + sanitizers + TSan + perf gate) green on the
  whole OS/compiler matrix incl. FreeBSD. mat is the fastest of the three across the bench
  scenarios — documented in a summary `bench/results/SUMMARY.md`.
