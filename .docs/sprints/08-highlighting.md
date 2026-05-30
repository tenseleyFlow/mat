# Sprint 08 — Syntax Highlighting (engine decided here)

## Goal
The deferred hard frontier: pick and ship a syntax-highlighting engine, chosen for
**speed**, that delivers bat-like colored output without bat's startup tax. This is a
multi-step effort; the engine decision is made at the *start* of this sprint with a short
spike, then implemented.

## Engine decision (speed-ranked options — choose during the opening spike)
1. **Hand-written lexers for the top ~10 languages** — fastest at runtime, no asset blob,
   most implementation effort; covers the bulk of real usage (audit 04 C3 option a).
2. **Compact precompiled grammar table, `mmap`'d at use** — broad language coverage with
   near-zero parse/startup cost (no decompress+deserialize) (audit 04 C3 option b, B1).
3. **Embed/port a TextMate engine** — widest coverage, closest to bat, heaviest and
   slowest; the fallback if breadth must match bat immediately (audit 04 C3 option c).
Decision recorded in `.docs/audits/05-highlight-engine.md` with the spike's benchmark
numbers driving the choice. Default lean: (2) for coverage-per-speed, with (1) for the
hottest languages.

## Targets / Deliverables
- `highlight/` subdir implementing the chosen engine behind a narrow interface
  (`highlight_line(syntax, theme, line) → styled spans`).
- **Lazy asset loading:** highlighting assets load **only when a TTY actually needs
  highlighting**, preferring `mmap` of a prebuilt table over decompress+deserialize —
  explicitly beating bat's ~800KB `OnceCell` startup cost (audit 02 §assets, audit 04 B1).
- Detection order wired from Sprint 07 (`-l` → map → ext → `#!` → plain).
- `--theme`, `--theme-dark`/`-light`, `--list-themes`, `-L/--list-languages`,
  `--fallback-syntax`.
- Theme/ANSI rendering completes the `interactive.c` path from Sprint 03.

## Pitfalls (cited)
- **Keep the 16KB long-line guard** regardless of engine — it exists because regex
  highlighting blows up O(n²) on minified/data lines; return unstyled past the threshold
  (audit 02 §highlighting).
- **Never on the cat-mode path.** Highlighting is TTY-and-asked-for only; piped output
  stays on the zero-copy/cooked paths (audit 04 C1).
- Don't load the asset blob at startup — laziness is the whole point of beating bat here
  (audit 04 B1).

## Architecture / Perf focus
- Highlight in parallel across lines/files where safe (sets up Sprint 09's pipeline).
- Asset format chosen for mmap-and-go, not parse-on-startup.
- Startup for non-highlight invocations is completely unaffected (tripwire still passes).

## Definition of Done
- `mat code.rs` on a TTY highlights correctly for the covered languages; `--theme`,
  `-L`, `--list-themes` work.
- Long-line guard verified (a >16KB line renders unstyled, fast, no hang).
- Startup tripwire still passes for non-highlight paths; a new bench shows mat's
  highlight-startup **≪ bat's** on a small source file (the headline beat-bat number),
  committed to `bench/results/08-*.md`.
- Engine choice + benchmarks documented in `audits/05-highlight-engine.md`.
