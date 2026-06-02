# Contributing to mat

Thanks for taking a look! mat is a from-scratch `cat`/`bat` written in C, and its
whole reason to exist is being *fast*. Contributions are very welcome — especially
anything that makes it quicker: shaving a syscall, tightening the hot path, a
smarter buffer policy, a better SIMD kernel. If you can show mat beating its old
self (or cat, or bat) on some workload, that's the best kind of PR you can send.

A few things that'll make your change easy to merge:

**Keep the plain path fast.** A bare `mat file > out` or `… | mat` is allocation-free,
stdio-free, and loads nothing it doesn't need — that's deliberate, and it's where mat
earns its keep. New features are great, but they belong behind the branch the plain
path never takes. If you touch anything on the hot path, bring a before/after number;
"it feels faster" doesn't cut it here — we've been burned by measurement noise enough
to insist on real numbers (and to never benchmark on `/dev/zero`).

**All tests must pass.** Run `make test` before you push. That's the unit tests, the
golden-output tests, and a byte-for-byte parity check against your system `cat` — mat
aims to be a drop-in replacement, so parity isn't negotiable. If you change behavior,
add or update a test that pins it. CI runs the same suite across Linux, macOS, and
FreeBSD plus the sanitizers, and trunk stays green.

Building and checking your work:

```sh
./configure        # no external deps; GNU make and BSD make both work
make
make test          # must be green before you commit
make fmt           # clang-format — CI pins clang-format-19, so match it
make asan          # optional: ASan/UBSan build for chasing memory bugs
make bench         # optional: hyperfine vs cat/bat
```

**Small commits, code that fits in.** Focused commits with terse, imperative subjects
are far easier to review than one big drop. Match the surrounding style — mat keeps one
concern per file and leans on clear memory and error ownership rather than cleverness.

That's the whole thing. If you'd like to talk an idea through first, open an issue — and
don't be shy about it. A small PR that makes mat measurably faster is always worth sending.
