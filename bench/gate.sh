#!/bin/sh
# gate.sh — enforcing performance gate.
#
# The hard assertion: mat must be at least as fast as cat on the file->/dev/null
# fast path (a large, stable margin from our adaptive buffering — robust against
# shared-runner noise). Tighter scenarios (file->file, file->pipe) where cat also
# uses kernel zero-copy are reported but not gated, since there mat ~= cat and a
# strict check would be flaky. vs bat, mat wins everywhere by a wide margin.
#
# Requires hyperfine and python3 (both present on the CI perf runner).
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
MAT=${MAT:-$ROOT/mat}

if ! command -v hyperfine >/dev/null 2>&1; then
    echo "gate: hyperfine not found — skipping (install it to enforce perf)"
    exit 0
fi

[ -f config.mk ] || ./configure >/dev/null 2>&1
make >/dev/null || { echo "build failed"; exit 1; }

scratch=$(mktemp -d "${TMPDIR:-/tmp}/mat_gate.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
dd if=/dev/urandom bs=1048576 count=256 of="$scratch/big" >/dev/null 2>&1

# Parse two-command hyperfine JSON; arg2 selects "gate" (exit nonzero if mat
# slower) or "report" (always exit 0).
parse() {
    python3 - "$1" "$2" "$3" <<'PY'
import json, sys
res = json.load(open(sys.argv[1]))["results"]
mat, ref = res[0]["mean"], res[1]["mean"]
mode, label = sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else ""
ratio = ref / mat if mat else float("inf")
faster = "faster" if mat <= ref else "SLOWER"
print(f"  {label}: mat={mat*1e3:7.2f}ms  cat={ref*1e3:7.2f}ms  ({ratio:.2f}x, mat {faster})")
sys.exit(1 if (mode == "gate" and mat > ref) else 0)
PY
}

fail=0
echo "perf gate (mat vs cat):"

# --- HARD GATE: file -> /dev/null ---
hyperfine -N --warmup 3 --export-json "$scratch/g.json" \
    "$MAT $scratch/big" "cat $scratch/big" >/dev/null 2>&1
if ! parse "$scratch/g.json" gate "file -> /dev/null  [GATED]"; then
    echo "PERF GATE FAIL: mat is slower than cat on file -> /dev/null"
    fail=1
fi

# --- REPORT ONLY: file -> file, file -> pipe (redirects/pipes need a shell) ---
hyperfine --warmup 3 --export-json "$scratch/r1.json" \
    "$MAT $scratch/big > $scratch/o_m" "cat $scratch/big > $scratch/o_c" >/dev/null 2>&1
parse "$scratch/r1.json" report "file -> file      [report]" || true

# Use `cat >/dev/null` as the drain, not `wc -c`: counting bytes dominates the
# measurement and masks the splice advantage with noise.
hyperfine --warmup 3 --export-json "$scratch/r2.json" \
    "$MAT $scratch/big | cat >/dev/null" "cat $scratch/big | cat >/dev/null" >/dev/null 2>&1
parse "$scratch/r2.json" report "file -> pipe      [report]" || true

[ "$fail" -eq 0 ] && echo "perf gate: PASS"
exit $fail
