# Audit 01 — `cat` Internals (BSD + GNU)

Sources read in full:
- `.docs/refs/freebsd-src/bin/cat/cat.c` + `cat.1` (BSD)
- `.docs/refs/coreutils/src/cat.c` (GNU)

This is the correctness + speed baseline `mat` must match and then beat. Two code
paths exist in both implementations: a **raw/fast path** (byte-for-byte copy, no
transformation) and a **cooked path** (any of `-b -n -s -t -e -v -A`).

---

## 1. Fast path (raw copy)

### Buffer sizing
- **BSD `raw_cat`**: regular→regular file uses physical-memory heuristic —
  `sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD (32K pages)` ⇒
  `MIN(BUFSIZE_MAX=2MB, MAXPHYS*8)`, else `BUFSIZE_SMALL=MAXPHYS (~256KB)`.
  Non-regular ⇒ `max(st_blksize, pagesize)`. Buffer cached across files.
- **GNU**: `io_blksize(&stat)` (wraps `st_blksize`), not memory-adaptive. In the
  cooked path the input buffer is grown to `max(insize, outsize)`.
- **Verdict for mat**: adopt BSD's adaptive sizing, cap ~1MB (diminishing returns
  above that). Respect `st_blksize` for pipes/devices.

### In-kernel zero-copy
- **BSD**: `copy_file_range(rfd,NULL,wfd,NULL,SSIZE_MAX,0)` in a loop; falls back to
  `raw_cat` on `EINVAL/EBADF/EISDIR`. No splice/sendfile/mmap.
- **GNU**: `copy_cat` uses `copy_file_range` in ~1GB chunks
  (`copy_max = (SSIZE_MAX>>30)<<30`), with a precise fallback errno set
  (`ENOSYS, ENOTSUP, EINVAL, EBADF, EXDEV, ETXTBSY, EPERM, EFBIG`). Tracks
  `some_copied` to distinguish "never started" (try next method) from "started then
  failed" (fatal). `splice_cat` handles pipes via a persistent intermediate pipe and
  `increase_pipe_size()`; only used for files > 32KB. Also `posix_fadvise(...,
  FADVISE_SEQUENTIAL)` per input file.
- **Verdict for mat**: fast-path decision tree
  `copy_file_range → splice → read/write(big buffer)`, with a GNU-style state
  machine to avoid re-attempting failed syscalls. `posix_fadvise` only for large
  files (>~10MB) to avoid a wasted syscall on small ones.

### Read/write loop
- **BSD** assumes partial writes (advances offset) but `err(1,...)` on any write
  error — crashes, no EINTR handling.
- **GNU** uses `full_write()` (retries partial writes + EINTR). Read errors are
  non-fatal (log, continue); write errors fatal.
- **Verdict for mat**: implement a `full_write()` equivalent (retry partial + EINTR;
  treat EAGAIN as error). Separate recoverable read errors from fatal write errors.

---

## 2. Cooked path (transformations)

### Line numbering `-n` / `-b`
- BSD uses `fprintf(stdout, "%6d\t", ++line)` — per-line `fprintf` overhead, `getc()`
  loop.
- GNU pre-formats a fixed `line_buf[20]` and increments it as a BCD-style decimal
  counter (`next_line_num`), copying with `stpcpy` — **much faster**, no per-line
  formatting. `-b` suppresses numbering on blank lines.
- **Verdict for mat**: GNU's pre-formatted counter buffer; 64-bit counter.

### Squeeze `-s`
- Both track consecutive newlines; GNU caps the counter at 2 to avoid overflow.
- **Verdict for mat**: counter capped at 2.

### Non-printable `-v` / `-e` / `-t` (and `-A`)
- BSD does per-char `putchar()` and is *wide-char aware* (`getwc`/`iswprint`/
  `putwchar`) — but its own man page admits `-t`/`-v` "does not recognize multibyte
  characters". The wide-char path is slow and a red herring.
- GNU is **byte-oriented**, writes via `*bpout++` into a big buffer (no `putchar`),
  simpler high-byte decode. Mapping: `<32 or 127` ⇒ `^X`/`^?`; `>=128` ⇒ `M-` +
  low-7-bit transform; tab ⇒ `^I` only under `-t`; newline ⇒ `$` under `-e`
  (CRLF ⇒ `^M$`).
- **Verdict for mat**: GNU's byte-oriented buffered approach. No wide-char for these
  flags. Output buffer over-allocated for worst-case 4× expansion
  (`insize*4 + outsize + LINE_COUNTER_BUF_LEN`); mat may use 2× + more aggressive
  flushing.

---

## 3. Correctness edge cases to replicate
- **input == output** (`SAME_INODE`, GNU only): compares dev+ino, excludes
  pipes/sockets/shm, then position-checks (`lseek SEEK_CUR/SEEK_END` for append) to
  catch `mat f > f`. Emits "input file is output file", continues.
- **stdin / `-`**: missing arg and `-` both mean stdin; track "did we read stdin" so
  we only `close()` it if opened; multiple `-` re-read stdin (POSIX).
- **`-u` unbuffered**: BSD `setbuf(stdout,NULL)`; modern GNU ignores it (always
  buffered). Pick one — likely implement honestly via direct `write()`.
- **exit status**: accumulate; 0 success, 1 on any file/read/write/close error.

---

## 4. Where mat can beat both (the "blazingly fast" thesis)
1. **SIMD scanning (SSE2/AVX2)** for newlines/special bytes — process 16/32 bytes per
   instruction; in cooked paths, bulk-copy runs of printable bytes and only branch on
   specials. Est. 2–4× on long lines.
2. **Adaptive large buffers** (BSD-style) instead of GNU's fixed `io_blksize`.
3. **No stdio in fast path** — `write()`/`full_write()` directly, skip output
   buffering entirely when no transform.
4. **256-entry expansion lookup table** for `-v`/`-t`/`-e` (each byte → output bytes +
   length) instead of branch chains.
5. **`vmsplice`** for cooked output to pipes (avoid final copy) — niche, big files.
6. **Lazy `setlocale`** — only when `-v` is active; skip otherwise.
7. **Lazy `posix_fadvise`** — only large files.

---

## mat fast-path decision tree (synthesized)
```
no transform flags?
  ├─ yes → try copy_file_range (1GB chunks, GNU errno fallback set)
  │         └─ fail-before-start → try splice (pipes, resize pipe)
  │                                  └─ fail-before-start → read()+full_write() big buf
  └─ no  → cooked path: big input buf → byte scan (SIMD-assisted) →
           expansion via lookup table + pre-formatted line counter → full_write()
```
