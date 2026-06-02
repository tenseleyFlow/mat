# mat benchmark — Sprint 00 baseline

Host: `Linux 5.15.0 x86_64`  CC: `cc`

## 256 MiB random file -> /dev/null
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.6rNBw2/big` | 16.8 ± 0.8 | 14.3 | 19.6 | 1.00 |
| `cat /tmp/claude-1001/mat_bench.6rNBw2/big` | 69.8 ± 6.0 | 58.8 | 88.1 | 4.15 ± 0.41 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.6rNBw2/big` | 208.7 ± 2.0 | 205.5 | 214.2 | 12.39 ± 0.62 |

## tiny file (startup-dominated)
| Command | Mean [µs] | Min [µs] | Max [µs] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.6rNBw2/tiny` | 715.2 ± 191.3 | 386.5 | 2180.5 | 1.36 ± 0.51 |
| `cat /tmp/claude-1001/mat_bench.6rNBw2/tiny` | 524.1 ± 136.9 | 373.2 | 1484.9 | 1.00 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.6rNBw2/tiny` | 1280.4 ± 276.8 | 899.2 | 2825.8 | 2.44 ± 0.83 |

## 256 MiB random file -> pipe (| wc -c)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.6rNBw2/big \| wc -c` | 72.5 ± 2.2 | 67.9 | 77.4 | 1.00 |
| `cat /tmp/claude-1001/mat_bench.6rNBw2/big \| wc -c` | 160.3 ± 5.1 | 153.5 | 175.7 | 2.21 ± 0.10 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.6rNBw2/big \| wc -c` | 943.6 ± 8.2 | 932.1 | 956.3 | 13.02 ± 0.41 |

## 128 MiB real source -> /dev/null, cat-compat -n (cooked + SIMD)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat -n /tmp/claude-1001/mat_bench.6rNBw2/text` | 80.7 ± 17.3 | 68.6 | 110.0 | 1.00 |
| `cat -n /tmp/claude-1001/mat_bench.6rNBw2/text` | 465.0 ± 2.3 | 462.0 | 468.6 | 5.76 ± 1.24 |

## decorated source -> /dev/null (render + highlight)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat --decorations=always --style=full --color=always --paging=never /tmp/claude-1001/mat_bench.6rNBw2/code.c` | 63.4 ± 1.0 | 62.1 | 67.5 | 1.00 |
| `bat --style=full --color=always --paging=never /tmp/claude-1001/mat_bench.6rNBw2/code.c` | 4742.8 ± 24.6 | 4686.3 | 4777.1 | 74.82 ± 1.24 |

_Data is random (never /dev/zero) for raw paths; real source for the
cooked and decorated paths. `-N` runs without a shell where possible._
