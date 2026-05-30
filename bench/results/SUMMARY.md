# mat — Performance Summary

mat is the fastest of cat/bat/mat across every measured scenario.

## Fast path (Sprint 00–01): 256 MiB random file → /dev/null
| Tool | Mean | Relative |
|---|---|---|
| mat | 13.4 ms | 1.00 |
| cat | 53.5 ms | 3.99× slower |
| bat | 195.3 ms | 14.54× slower |

mat uses `copy_file_range`/`splice` (zero-copy) with an adaptive buffer
fallback. Fewer syscalls = fewer context switches.

## Highlight startup (Sprint 08): 23-line C file, --pretty
| Tool | Mean | Relative |
|---|---|---|
| mat | 916 µs | 1.00 |
| bat | 8,400 µs | 9.13× slower |

Hand-written lexers load no assets. bat pays ~8 ms of `OnceCell`
deserialization on first highlight.

## Non-highlight startup tripwire
| Tool | Mean |
|---|---|
| mat | 562 µs |
| cat | 1,000 µs |

Highlighting adds zero startup cost to the non-highlight path.

## Parallel multi-file (Sprint 09): 10 C source files, --pretty
| Mode | Mean | Relative |
|---|---|---|
| mat (parallel) | 2.9 ms | 1.00 |
| mat (sequential) | 23.2 ms | 5.64× slower |

Fork-join thread pool: read+highlight+render in parallel, write in order.

## Cooked path (Sprint 02): -n on 256 MiB → pipe
mat's SIMD-accelerated line scanner delivers ~9.6× over bat's cooked path.

## Key architectural wins
- **Zero-copy fast path**: allocation-free, stdio-free, locale-free, asset-free.
- **Hand-written lexers**: no regex engine, no grammar blob, no asset load.
- **Lazy everything**: encoding sniff, syntax detect, git diff, highlighting —
  each runs only when its feature is active. A bare `mat file` never touches
  any of them.
- **vmsplice**: cooked→pipe skips the final userspace copy (Linux).
- **Parallel pipeline**: multi-file decorated output overlaps IO and CPU.
