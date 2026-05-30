# Audit 02 — bat Rendering / Output Pipeline

Source: `.docs/refs/bat/src/` (controller, printer, decorations, input, output,
preprocessor, assets). This is the model `mat` imitates *incrementally* once cat
parity is solid. Key file:line anchors are kept for when we implement each piece.

---

## Control flow (file → output)
`Controller::run` (controller.rs:39) forks on `config.loop_through`
(controller.rs:192):
- **`loop_through = true` → `SimplePrinter`**: raw, no color/decoration/highlight.
  This is the "act like cat" mode (piped output / `--no-paging`). *mat's fast path
  maps here.*
- **`loop_through = false` → `InteractivePrinter`**: highlight + decorations + grid +
  wrapping. *mat's eventual fancy path.*

Paging decision (controller.rs:57): output is a TTY **and** there are file inputs
**and** `paging_mode ∈ {Always, QuitIfOneScreen}` → route through pager
(`output.rs`).

Line loop (controller.rs:252): buffered read with lookahead (a `VecDeque` sized to
the largest line-range offset) so `--range` can know EOF; each line goes to
`printer.print_line(out_of_range, ...)`.

**Takeaway for mat**: decide TTY-vs-pipe *early*, branch to a simple writer vs a
decorated writer. The two-tier model is the single most important structural idea to
copy.

---

## Decoration / panel model (decorations.rs, printer.rs:216)
- `Decoration` trait = `{ generate(line_no, continuation, printer) -> text; width() }`.
- Decorations pushed in order: `LineNumberDecoration` (width 4, grows past 10000),
  `LineChangesDecoration` (git, width 1). Then:
  `panel_width = decorations.len() + Σ width()` (the `.len()` term = one space per
  decoration). **Grid border `│` is pushed *after* computing panel_width** to avoid
  circular dependency.
- Panel disabled entirely if `term_width < panel + 5` (printer.rs:261).
- Grid also draws horizontal rules with `─` and junction chars `┬ ┼ ┴` at the panel
  boundary (printer.rs:352).
- Git markers (decorations.rs:103): pre-rendered width-1 glyphs `+ ~ ‾ _` / space.

**Takeaway for mat**: model the gutter as a small array of decoration descriptors
each exposing `width` + a `render(line_no)` callback (function pointers / a tiny
vtable struct in C). Compute panel width once. This is clean to port.

---

## Syntax highlighting (printer.rs:432, assets.rs)
- Assets are a **precompiled, brotli-compressed binary blob**: `syntaxes.bin` (~1MB),
  `themes.bin` (~58KB), lazily deserialized via `OnceCell` on first use (~800KB in
  memory). This deserialization is the dominant startup cost for small inputs.
- Detection order: explicit `-l` → `--map-syntax` glob → extension → first-line
  (`#!`) → Plain Text fallback.
- Per line: `HighlightLines::highlight_line` → `Vec<(Style, &str)>`. **Lines > 16KB
  skip highlighting** (returns unstyled) to dodge regex O(n²) blowup on minified/data
  files. This guard is essential.
- syntect is a TextMate-grammar regex FSM (Oniguruma or fancy-regex). Heavyweight:
  grammar load + regex compile on first line of each syntax.

**Takeaway for mat**: highlighting is the expensive, hard part — **defer it**. When we
get there, options are (a) embed/port a TextMate engine, (b) build a cheaper
hand-written lexer for a handful of popular languages, or (c) shell to an existing
engine. Keep the 16KB long-line guard. Decision deferred to a later sprint per the
phased plan.

---

## Non-printable / `--show-all` (preprocessor.rs:59)
`replace_nonprintable(input, tab_width, notation)` runs *before* highlighting for both
printers. UTF-8 aware. Space → `·`; tab → `↹` or `├──┤` box-drawn to the tab stop;
control chars → caret `^@..^?` or Unicode `␀..␡` depending on `--nonprintable-notation`;
invalid UTF-8 → `\xHH`. Char display width via `unicode_width` (control = 2 cells).

Note this is *bat's* prettier rendering; it differs from `cat -v`'s `^X`/`M-` bytes.
mat must support **both**: `cat -v` byte semantics (Audit 01) for cat-compat, and
optionally bat's Unicode glyphs for the fancy path.

---

## Input buffering (input.rs:265)
- Eagerly reads the **first line** to sniff content type (`content_inspector`:
  UTF-8 / UTF-16LE/BE with BOM / BINARY via `PK\x03\x04` etc. / EMPTY), then keeps the
  `BufReader` for the rest. UTF-16 lines re-read with the correct terminator.
- Files: `BufReader::new(File)`. Stdin: `BufReader::new(stdin_lock())`. Unified
  `InputReader::read_line` downstream; `unbuffered` mode uses `fill_buf`+`consume` for
  `tail -f` latency.

**Takeaway for mat**: sniff a leading chunk for binary/encoding; for 2026 we can make
UTF-8 the happy path and treat UTF-16/binary as special cases, saving the two-pass
overhead bat pays.

---

## Output / paging (output.rs)
- `OutputType ∈ { Pager(child less), BuiltinPager(minus), Stdout }`.
- `less` invoked with `-R` (ANSI), `-F` (quit if one screen), `-S` (chop), `-K` (quit
  on interrupt), `LESSCHARSET=UTF-8`, with version-sniffing workarounds for old less.
- No buffering layer; writes per line segment.

**Takeaway for mat**: paging = fork/exec `less -RFS` with stdin pipe. Cheap to add;
a builtin pager is a later luxury.

---

## Where bat is slow vs cat (mat's opening)
1. **No `copy_file_range`/splice fast path** — every byte goes read→highlight→write
   even in cat mode. mat's C fast path is the headline win.
2. Syntax-set deserialization (~800KB) on startup.
3. syntect regex highlighting per line; UTF-16 two-pass; per-file `git diff` subprocess
   for change markers; wrapping regenerates decoration strings per wrapped line.

mat's strategy: **be cat-fast by default (C + zero-copy), pay for prettiness only when
a TTY asks for it, and keep highlighting lazy/optional.**
