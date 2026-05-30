# mat benchmark — Sprint 00 baseline

Host: `Linux 5.15.0 x86_64`  CC: `cc`

## 256 MiB random file -> /dev/null
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.1BXl1E/big` | 13.4 ± 1.1 | 12.0 | 20.8 | 1.00 |
| `cat /tmp/claude-1001/mat_bench.1BXl1E/big` | 53.5 ± 6.4 | 47.3 | 62.8 | 3.99 ± 0.58 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.1BXl1E/big` | 195.3 ± 1.1 | 193.8 | 198.4 | 14.54 ± 1.22 |

## tiny file (startup-dominated)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.1BXl1E/tiny` | 1.1 ± 0.5 | 0.3 | 2.6 | 1.00 |
| `cat /tmp/claude-1001/mat_bench.1BXl1E/tiny` | 1.4 ± 0.7 | 0.4 | 2.6 | 1.34 ± 0.94 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.1BXl1E/tiny` | 3.2 ± 1.4 | 1.0 | 7.3 | 3.02 ± 2.03 |

## 256 MiB random file -> pipe (| wc -c)
| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `/home/mfwolffe/GithubOrgs/tenseleyFlow/mat/mat /tmp/claude-1001/mat_bench.1BXl1E/big \| wc -c` | 130.6 ± 38.8 | 60.3 | 185.7 | 1.00 |
| `cat /tmp/claude-1001/mat_bench.1BXl1E/big \| wc -c` | 149.0 ± 5.2 | 141.5 | 159.8 | 1.14 ± 0.34 |
| `bat --paging=never --style=plain /tmp/claude-1001/mat_bench.1BXl1E/big \| wc -c` | 937.8 ± 14.5 | 913.1 | 951.9 | 7.18 ± 2.14 |

_Data is random (never /dev/zero). `-N` runs without a shell where possible._
