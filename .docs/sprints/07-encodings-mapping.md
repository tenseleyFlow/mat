# Sprint 07 — Encodings & File Mapping

## Goal
Handle non-UTF-8 input, binary detection, ANSI passthrough/stripping, and filename→syntax
mapping — making UTF-8 the zero-overhead happy path and everything else an explicit,
opt-in slow branch. Prepares the ground for highlighting (Sprint 08).

## Targets / Deliverables
- **`encoding.c`:** single leading-block content sniff (UTF-8 / UTF-16LE/BE via BOM /
  BINARY via signatures like `PK\x03\x04` / EMPTY). UTF-8 is the fall-through happy path
  with no extra work; UTF-16 decoded in an explicit branch; binary handled per `--binary
  {no-printing,as-text}` (audit 02 §input).
- **`--strip-ansi {auto,always,never}`** — parse/strip input ANSI escapes; `auto` keeps
  them for plain text, strips under highlighting (audit 02 §printer ANSI).
- **Syntax mapping inputs** (consumed by Sprint 08): `-m/--map-syntax` glob→syntax,
  `--ignored-suffix`, `--file-name` (display name + detection for stdin),
  `--fallback-syntax`. Detection order assembled: explicit `-l` → map-syntax → extension →
  first-line `#!` → plain (audit 02 §detection).

## Pitfalls (cited)
- **Don't pay bat's two-pass cost on the UTF-8 path.** bat eagerly reads then re-reads the
  first line for UTF-16/BOM handling; mat sniffs one block and only re-decodes when the
  encoding is actually UTF-16 (audit 02 §input.rs:265, audit 04 C2).
- Binary files default to skip-with-notice (`no-printing`), matching bat — don't dump raw
  bytes by default.
- Glob matching for `--map-syntax` should be precompiled once, not per file (audit 02 §map).

## Architecture / Perf focus
- UTF-8 happy path is overhead-free; UTF-16/binary/ANSI-strip are branches the common case
  never enters (audit 04 C2).
- This sprint adds detection/decoding *inputs*; it does not yet highlight.

## Definition of Done
- Fixtures for UTF-16LE/BE (with BOM), a binary file, an ANSI-laden file; tests assert
  correct detection, `--binary` behavior, and `--strip-ansi` modes.
- `-m`/`--ignored-suffix`/`--file-name`/`--fallback-syntax` resolve to the right syntax
  name (verified via a debug `--detect-syntax`-style probe or list output).
- Parity unaffected for plain UTF-8/ASCII; startup unchanged on the UTF-8 path (tripwire).
