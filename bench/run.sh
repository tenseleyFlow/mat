#!/bin/sh
# run.sh — benchmark mat against cat and bat.
#
# Prefers hyperfine; degrades to a coarse timed loop if it is absent. Writes a
# Markdown summary to bench/results/. "Blazingly fast" is a measured claim — this
# is where we measure it (audit 04 E).
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
MAT=${MAT:-$ROOT/mat}
OUT=bench/results/00-baseline.md
mkdir -p bench/results

[ -f config.mk ] || ./configure >/dev/null 2>&1
make >/dev/null || { echo "build failed"; exit 1; }

scratch=$(mktemp -d "${TMPDIR:-/tmp}/mat_bench.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM

# 64 MiB regular file, and a many-small-files directory.
dd if=/dev/zero bs=1048576 count=64 of="$scratch/big" >/dev/null 2>&1
mkdir "$scratch/many"
i=0; while [ "$i" -lt 200 ]; do printf 'line %d\n' "$i" > "$scratch/many/f$i"; i=$((i+1)); done
printf 'tiny\n' > "$scratch/tiny"

have() { command -v "$1" >/dev/null 2>&1; }

{
    echo "# mat benchmark — Sprint 00 baseline"
    echo
    echo "Host: \`$(uname -srm)\`  CC: \`${CC:-cc}\`"
    echo
} > "$OUT"

if have hyperfine; then
    BAT=""
    have bat && BAT="bat --paging=never --style=plain"
    run_case() { # title; file
        title=$1; f=$2
        echo "## $title" >> "$OUT"
        if [ -n "$BAT" ]; then
            hyperfine -N --warmup 3 --export-markdown "$scratch/h.md" \
                "$MAT $f" "cat $f" "$BAT $f" >/dev/null 2>&1 || true
        else
            hyperfine -N --warmup 3 --export-markdown "$scratch/h.md" \
                "$MAT $f" "cat $f" >/dev/null 2>&1 || true
        fi
        cat "$scratch/h.md" >> "$OUT" 2>/dev/null
        echo >> "$OUT"
    }
    run_case "64 MiB file -> /dev/null" "$scratch/big"
    run_case "tiny file (startup-dominated)" "$scratch/tiny"
    echo "_Note: pipe redirection handled by the shell; -N disables hyperfine's own shell._" >> "$OUT"
else
    echo "hyperfine not found — recording a coarse timed loop instead." >> "$OUT"
    echo '```' >> "$OUT"
    for tool in "$MAT" cat; do
        t0=$(date +%s 2>/dev/null)
        n=0; while [ "$n" -lt 20 ]; do "$tool" "$scratch/big" >/dev/null 2>&1; n=$((n+1)); done
        t1=$(date +%s 2>/dev/null)
        echo "$(basename "$tool"): 20x 64MiB in $((t1 - t0))s" >> "$OUT"
    done
    echo '```' >> "$OUT"
fi

echo "wrote $OUT"
cat "$OUT"
