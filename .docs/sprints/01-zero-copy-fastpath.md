# Sprint 01 — Zero-Copy Fast Path

## Goal
Turn the naive `read()`+`write()` copy from Sprint 00 into the headline raw-throughput
win: a kernel zero-copy ladder that pushes file→file and file→pipe at near-device
bandwidth, with a state machine that never wastes a syscall re-attempting a method the
kernel already refused. This is where `mat` first decisively out-runs `cat`.

## Targets / Deliverables
- **`fastpath.c` ladder:** `copy_file_range → splice → read()+full_write()`, tried in
  order, each gated by capability + the input/output kind.
  - `copy_file_range` in ~1GB chunks (`copy_max = (SSIZE_MAX>>30)<<30`, audit 01 §1).
  - `splice` for pipe endpoints via a persistent intermediate pipe + `fcntl(F_SETPIPE_SZ)`
    growth; only for inputs > 32KB (audit 01 §1).
  - `sendfile` for regular→socket where available.
- **Syscall-class state machine:** track whether a method *ever started* (`some_copied`)
  to distinguish "never started → fall back to next method" from "started then failed →
  fatal" (audit 01 §1). Once a method's class fails with a fallback errno, don't retry it
  for the rest of the run.
- **`compat.h` + `configure` probes:** compile-tests set `HAVE_COPY_FILE_RANGE`,
  `HAVE_SPLICE`, `HAVE_SENDFILE`, `HAVE_VMSPLICE`, plus `MAXPHYS`/blocksize constants.
  Fallback decls where a libc lacks the wrapper.
- **`SAME_INODE` guard:** detect input==output (dev+ino, excluding fifo/sock/shm), then
  position-check (`lseek SEEK_CUR`, `SEEK_END` for `O_APPEND`) to catch `mat f > f`;
  emit "input file is output file", continue (audit 01 §3).
- **Lazy `posix_fadvise(SEQUENTIAL)`** for large inputs only (audit 04 B2).
- **`bench/baseline.json`** captured; the CI **perf gate flips to enforcing** (regression
  vs baseline beyond threshold fails; assert mat ≥ cat on the fast path).

## Pitfalls (cited)
- **Exact fallback errno set** for `copy_file_range`: `ENOSYS, ENOTSUP, EINVAL, EBADF,
  EXDEV, ETXTBSY, EPERM, EFBIG` → try next method; anything else after bytes moved is
  fatal (audit 01 §1).
- **BSD vs Linux divergence:** FreeBSD has `copy_file_range` but not Linux `splice`;
  Linux has both. This is precisely why the FreeBSD CI job exists — the ladder must
  compile and pass there with a different capability set (audit 01 §1, audit 03 CI).
- **Pipe-to-pipe** can't always splice directly — keep GNU's intermediate-pipe approach.
- Don't `fadvise` tiny files (wasted syscall — audit 04 B2).

## Architecture / Perf focus
- Fewer syscalls is the KPI: track `strace -c` deltas vs Sprint 00 (audit 04 A3, E).
- The ladder lives entirely behind the fast-path branch; the hot-path invariant holds
  (no malloc/stdio/locale).
- `vmsplice` for cooked→pipe is noted here but lands with the cooked path / Sprint 09.

## Definition of Done
- `make test` green incl. a new `test_fastpath` (capability gating, state-machine
  transitions via injected fake errno) and parity unchanged.
- `parity.sh` still byte-identical to `cat`, including the `mat f > f` self-overwrite
  case (matches cat's refusal/behavior).
- `bench` shows mat **≥ cat** and **≫ bat** on file→file and file→pipe; `strace -c`
  confirms a syscall-count drop vs Sprint 00. Numbers in `bench/results/01-*.md`.
- Green on all three OSes incl. FreeBSD's distinct syscall set; perf gate enforcing.
