# mat(t)

A from-scratch C11/POSIX reimplementation of `cat` with syntax highlighting, a
built-in pager, line numbers, git diff markers, and a decoration frame. Outputs
are byte-identical to `cat` when no decoration flags are used.

## Performance

Measured with [hyperfine](https://github.com/sharkdp/hyperfine) on a 256 MiB
file of random data (not `/dev/zero`, which flatters any reader). All numbers
are means over 30+ runs with warmup.

### Throughput (256 MiB random file to /dev/null)

| Command | Time | vs mat |
|:--------|-----:|-------:|
| `mat` | 26 ms | 1.0x |
| `cat` | 61 ms | 2.3x slower |
| `bat --style=plain` | 195 ms | 7.4x slower |

### Throughput (256 MiB random file to a pipe)

| Command | Time | vs mat |
|:--------|-----:|-------:|
| `mat \| wc -c` | 132 ms | 1.0x |
| `cat \| wc -c` | 148 ms | 1.1x slower |
| `bat --style=plain \| wc -c` | 927 ms | 7.0x slower |

### Highlighted output (small C source file)

| Command | Time | vs mat |
|:--------|-----:|-------:|
| `mat --pretty` | 1.8 ms | 1.0x |
| `bat --style=full` | 9.3 ms | 5.2x slower |

mat uses hand-written lexers with no grammar files or asset blobs. There is no
deserialization step at startup.

### Highlighted output (1000-line text file)

| Command | Time | vs mat |
|:--------|-----:|-------:|
| `mat --pretty` | 2.2 ms | 1.0x |
| `bat --style=full` | 6.6 ms | 3.0x slower |

### Highlighted output (large source file, ~4700 lines)

| Command | Time | vs mat |
|:--------|-----:|-------:|
| `mat --pretty` | 4.0 ms | 1.0x |
| `bat --style=full` | 158 ms | 39.6x slower |

The gap widens on larger files because bat's TextMate regex engine scales
with line count, while mat's hand-written lexers are a single-pass char scan.

### Startup (one-line file, plain output)

| Command | Time |
|:--------|-----:|
| `cat` | 1.7 ms |
| `mat` | 2.0 ms |
| `bat --style=plain` | 3.9 ms |

Plain `mat` has comparable startup to `cat`. Adding highlighting does not change
this number because the lexers load nothing.

## Install

**Homebrew** (macOS / Linux):
```sh
brew tap tenseleyFlow/tap
brew install mat
```

**AUR** (Arch Linux):
```sh
yay -S mat-cat
```

**From source:**
```sh
./configure        # probes syscalls (copy_file_range, splice, vmsplice, fadvise)
make               # builds ./mat (GNU make and BSD make)
make test          # unit tests + integration goldens + byte-exact cat parity
make bench         # hyperfine vs cat and bat
make asan          # ASan/UBSan build
sudo make install  # installs to /usr/local by default
```

**Pre-built binaries** are available on the
[releases page](https://github.com/tenseleyFlow/mat/releases) for Linux
(x86_64) and macOS (arm64).

Requires a C11 compiler and POSIX. No external libraries. Tested on Linux,
macOS, and FreeBSD.

## Usage

```sh
mat file.txt                      # print a file (identical to cat)
mat -n file.txt                   # number lines (identical to cat -n)
mat -s -A file.txt                # squeeze blanks + show-all (identical to cat -sA)
mat --pretty file.c               # line numbers, grid, header, syntax highlighting
mat --pretty --theme=dracula *.py # colored output with a named theme
mat --pretty -d file.c            # git change markers in the gutter
mat -r 10:20 file.txt             # print only lines 10 through 20
mat -r -5: file.txt               # last 5 lines (without buffering the whole file)
mat -H 15 --pretty file.c         # highlight line 15
mat --detect-syntax file.rs       # print the resolved syntax name
```

### Decoration flags

| Flag | Effect |
|:-----|:-------|
| `-p, --pretty` | Full frame: header, line numbers, grid, highlighting |
| `--style=LIST` | Fine-grained: `numbers,grid,header,header-filesize,rule,snip` |
| `--color=WHEN` | `auto`, `never`, `always` |
| `--theme=NAME` | Color theme (see `--list-themes`) |
| `-d, --diff` | Show git change markers (`+` added, `~` modified, `_` removed) |
| `--wrap=MODE` | `auto`, `never`, `character`, `word` |
| `--tabs=N` | Tab expansion width (default 4 in decorated mode) |
| `-S` | Do not wrap long lines |
| `-P, --no-paging` | Disable the built-in pager |

### Line selection

| Flag | Effect |
|:-----|:-------|
| `-r N:M` | Print lines N through M |
| `-r :M` | First M lines |
| `-r N:` | Line N to end |
| `-r -N:` | Last N lines |
| `-r N:+M` | N lines starting at N |
| `-r N::C` | Line N with C lines of context |
| `-H N:M` | Highlight lines N through M (decorated output) |
| `--squeeze-limit=N` | Max consecutive blank lines under `-s` |

### Encoding and syntax

| Flag | Effect |
|:-----|:-------|
| `--binary=WHEN` | `no-printing` (default) or `as-text` |
| `--strip-ansi=WHEN` | Remove input ANSI escapes (`auto` strips under highlighting) |
| `-l NAME` | Force a syntax |
| `-m GLOB:NAME` | Map filename patterns to syntaxes |
| `--ignored-suffix=.EXT` | Strip suffix before extension detection |
| `--file-name=NAME` | Set display name and syntax detection for stdin |
| `-L` | List supported languages |
| `--list-themes` | List available themes |
| `--detect-syntax` | Print the detected syntax for each input |

### cat compatibility

All standard `cat` flags work: `-n`, `-b`, `-s`, `-e`, `-t`, `-v`, `-A`, `-E`,
`-T`. When none of the decoration flags are active, output is byte-identical to
GNU `cat` (verified by 74 parity test cases on every commit).

## Languages

mat includes hand-written lexers for 130 languages, covering all common
programming languages, markup formats, config files, and shell scripting
languages. Run `mat -L` for the full list. A few examples:

Ada, Assembly, AWK, Bash, C, C#, C++, Clojure, CSS, Dart, Dockerfile,
Elixir, Erlang, F#, Fish, Fortran, GLSL, Go, GraphQL, Groovy, Haskell,
HTML, Java, JavaScript, JSON, Julia, Kotlin, LaTeX, Lisp, Lua, Makefile,
Markdown, MATLAB, Nim, Nix, Objective-C, OCaml, Pascal, Perl, PHP,
PowerShell, Protobuf, Python, R, Ruby, Rust, Scala, Solidity, SQL, Svelte,
Swift, Tcl, Terraform, TOML, TypeScript, Verilog, VimL, Vue, WGSL, XML,
YAML, Zig, Zsh

Languages without a lexer render with the decoration frame but no syntax
coloring.

## Themes

45 built-in themes, each sourced from its official spec. Run `mat --list-themes`
for the full list. Set a default in your config file with `--theme=NAME`.

**Dark:** dark (default), dracula, github-dark, gruvbox, kanagawa, material,
monokai, moonfly, nightfly, nightfox, nightowl, nord, onedark, oxocarbon,
palenight, poimandres, synthwave, tokyonight, tomorrow-night, zenburn

**Light:** ayu-light, catppuccin-latte, dayfox, everforest-light, github-light,
gruvbox-light, light, modus-operandi, onelight, rosepine-dawn, solarized-light,
tomorrow

**Mid/Muted:** ayu-dark, ayu-mirage, carbonfox, catppuccin, catppuccin-frappe,
catppuccin-macchiato, dawnfox, everforest-dark, iceberg, modus-vivendi, rosepine,
rosepine-moon, solarized-dark

## Pager

mat includes a built-in terminal pager
([paige](https://github.com/tenseleyFlow/paige)) that activates automatically
for decorated output on a terminal. Navigation:

| Key | Action |
|:----|:-------|
| `j` / `k` / arrows | Scroll one line |
| `space` / `f` / `b` | Page down / up |
| `d` / `u` | Half page down / up |
| `g` / `G` | Top / bottom |
| digits | Jump to line (type `16` to go to line 16; pause resets) |
| `q` | Quit |

## How it works

`main.c` decides once which pipeline runs:

1. **Fast path** (no flags, or only `-u`): zero-copy I/O via `copy_file_range`,
   `splice`, or a tight `read`/`write` loop. Allocation-free, stdio-free,
   locale-free. This is why plain `mat` is faster than `cat`.

2. **Cooked path** (`-n`, `-b`, `-s`, `-v`, `-e`, `-t`, `-A`): a faithful port
   of GNU cat's byte loop with SIMD-accelerated newline scanning.

3. **Decorated path** (`--pretty`, `--style`, `--color`): line numbers, grid,
   header, syntax highlighting, wrapping, git diff markers. When there are
   multiple files, each is highlighted in a separate thread and the results are
   written in order.

The fast path never loads highlighting assets, never calls `malloc`, and never
touches the decorated code. The cooked path is byte-identical to GNU `cat`. The
decorated path only runs when explicitly requested or when output is to a
terminal with `--style` set.

## Compared to cat and bat

**cat** is the baseline. mat matches its output byte-for-byte and is faster on
bulk throughput because it uses zero-copy kernel calls (`copy_file_range`,
`splice`) that cat's `read`/`write` loop does not.

**bat** provides syntax highlighting, line numbers, git integration, and a
pager. mat provides the same features. The performance difference comes from the
highlighting engine: bat loads and deserializes a set of TextMate grammar files
(~8 ms on first use), while mat's hand-written lexers are compiled into the
binary and require no loading step. mat covers 130 languages and 45 themes where
bat covers ~174 languages and ~25 themes. For languages mat does not cover, the
file renders with the decoration frame but without syntax coloring.

Both mat and bat fall back to plain output when piped. Both support `--paging`,
`--style`, `--color`, `-r` (line ranges), and themes.

## CI

Every push runs: clang-format lint, build matrix (Linux gcc + clang, macOS
clang), FreeBSD VM, ASan/UBSan, TSan (thread sanitizer for the parallel
pipeline), performance gate (mat must beat cat on file-to-pipe throughput), and
benchmarks.

## License

MIT. See [LICENSE](LICENSE).
