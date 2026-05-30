# mat

`cat`/`bat`, but blazingly fast. A from-scratch C11/POSIX reimplementation of
`cat` with `bat`'s niceties, built to be the fastest of the three — and to
*innovate past* both wherever they leave performance on the table.

> Status: **Sprint 00** — project scaffold + a correct, fast `cat`. The
> zero-copy ladder, SIMD cooked path, decorations, paging, and syntax
> highlighting land in subsequent sprints.

## Build

```sh
./configure        # probes the toolchain + syscalls, writes config.mk + config_generated.h
make               # builds ./mat  (works under GNU make and BSD make)
make test          # unit (Unity) + integration goldens + byte-exact parity vs cat
make bench         # benchmark vs cat/bat (uses hyperfine if present)
sudo make install  # installs to $PREFIX/bin (default /usr/local)
```

Tested on FreeBSD, Linux, and macOS. No external build dependencies.

## Usage

```sh
mat file.txt              # print a file
mat a.txt b.txt           # concatenate
some_cmd | mat            # read stdin
mat a.txt - b.txt         # mix files and stdin
```

Run `mat --help` for the full (growing) option list.

## Design

`mat` splits at the top of `main` into a **fast path** (no transformation:
allocation-free, stdio-free, kernel zero-copy) and, in later sprints, a
**cooked / interactive path** (numbering, `--show-all`, decorations, paging,
highlighting) reached only when flags or a TTY ask for it. The common case —
piping or redirecting a file — never pays for prettiness it isn't using.

Development is organized as sprints; the planning docs, reference audits, and
the speed-innovation catalogue live under `.docs/` (kept locally, not tracked).

## License

See [LICENSE](LICENSE).
