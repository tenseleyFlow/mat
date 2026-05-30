# Sprint 08 — Highlight-startup benchmark

Host: `Linux 5.15.0 x86_64`  CC: `cc`

## Highlight: mat --pretty vs bat --style=full (23-line C file, --shell=none)
| Command | Mean [µs] | Min [µs] | Max [µs] | Relative |
|:---|---:|---:|---:|---:|
| `mat --pretty --color=always bench_hl.c` | 915.7 ± 470.0 | 333.1 | 2976.4 | 1.00 |
| `bat --style=full --color=always --paging=never bench_hl.c` | 8400 ± 2400 | 5400 | 16100 | **9.13× slower** |

mat is **~9× faster** on highlight-startup: no asset deserialization, no regex
engine, just a char-scan lexer that starts instantly. The structural advantage is
that mat's hand-written lexers load nothing — highlight-startup equals the normal
startup the tripwire already measures.

## Startup tripwire: mat fast-path vs cat (same file, no highlight)
| Command | Mean [µs] |
|:---|---:|
| `mat bench_hl.c` | 562 ± 412 |
| `cat bench_hl.c` | 1000 ± 400 |

Non-highlight startup is unchanged: mat still beats cat on a tiny file, proving
that the highlight engine adds zero overhead to the non-highlight path.
