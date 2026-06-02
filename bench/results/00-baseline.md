# mat benchmark — Sprint 00 baseline

Host: `Linux 5.15.0 x86_64`  CC: `cc`

## 256 MiB random file -> /dev/null
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.m3srEZ/big` | 16.4 ± 2.4 | 14.3 | 39.6 | 1.00 |
| `cat /tmp/claude-1001/mat_bench.m3srEZ/big` | 68.6 ± 1.0 | 64.3 | 70.9 | 4.18 ± 0.61 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.m3srEZ/big` | 208.2 ± 5.9 | 203.9 | 227.9 | 12.70 ± 1.89 |

## tiny file (startup-dominated)
| Command | Mean [µs] | Min [µs] | Max [µs] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.m3srEZ/tiny` | 875.9 ± 255.7 | 378.9 | 2093.7 | 1.10 ± 0.52 |
| `cat /tmp/claude-1001/mat_bench.m3srEZ/tiny` | 793.9 ± 294.8 | 360.1 | 2105.9 | 1.00 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.m3srEZ/tiny` | 1693.5 ± 484.5 | 892.1 | 4634.8 | 2.13 ± 1.00 |

## 256 MiB random file -> pipe (| wc -c)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.m3srEZ/big \| wc -c` | 86.8 ± 13.1 | 67.9 | 112.9 | 1.00 |
| `cat /tmp/claude-1001/mat_bench.m3srEZ/big \| wc -c` | 156.3 ± 1.5 | 154.5 | 160.0 | 1.80 ± 0.27 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.m3srEZ/big \| wc -c` | 945.5 ± 14.7 | 928.8 | 973.4 | 10.90 ± 1.66 |

## 128 MiB real source -> /dev/null, cat-compat -n (cooked + SIMD)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat -n /tmp/claude-1001/mat_bench.m3srEZ/text` | 98.1 ± 0.9 | 96.3 | 100.1 | 1.00 |
| `cat -n /tmp/claude-1001/mat_bench.m3srEZ/text` | 461.7 ± 4.1 | 458.1 | 468.7 | 4.70 ± 0.06 |

## decorated source -> /dev/null (render + highlight)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat --decorations=always --style=full --color=always --paging=never /tmp/claude-1001/mat_bench.m3srEZ/code.c` | 80.6 ± 0.7 | 79.8 | 83.9 | 1.00 |
| `bat --style=full --color=always --paging=never /tmp/claude-1001/mat_bench.m3srEZ/code.c` | 4652.3 ± 33.2 | 4618.1 | 4716.8 | 57.74 ± 0.65 |

_Data is random (never /dev/zero) for raw paths; real source for the
cooked and decorated paths. `-N` runs without a shell where possible._
