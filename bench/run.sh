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

# 256 MiB of RANDOM data — never /dev/zero: tmpfs zero-pages flatter any reader
# and produce dishonest (inflated) throughput numbers.
dd if=/dev/urandom bs=1048576 count=256 of="$scratch/big" >/dev/null 2>&1 \
    || dd if=/dev/random bs=1048576 count=256 of="$scratch/big" >/dev/null 2>&1
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
    run_case "256 MiB random file -> /dev/null" "$scratch/big"
    run_case "tiny file (startup-dominated)" "$scratch/tiny"

    # Pipe scenario needs a shell, so drop -N for this one.
    echo "## 256 MiB random file -> pipe (| wc -c)" >> "$OUT"
    if [ -n "$BAT" ]; then
        hyperfine --warmup 2 --export-markdown "$scratch/h.md" \
            "$MAT $scratch/big | wc -c" "cat $scratch/big | wc -c" \
            "$BAT $scratch/big | wc -c" >/dev/null 2>&1 || true
    else
        hyperfine --warmup 2 --export-markdown "$scratch/h.md" \
            "$MAT $scratch/big | wc -c" "cat $scratch/big | wc -c" >/dev/null 2>&1 || true
    fi
    cat "$scratch/h.md" >> "$OUT" 2>/dev/null
    echo >> "$OUT"

    # Cooked path: line numbering, the SIMD-accelerated transform (mat vs cat;
    # bat's -n output differs, so it is excluded from this comparison).
    # Use REAL source text, not `yes`: a repeated single line is perfectly
    # branch-predictable and flatters the cooked loop. The project's own
    # sources have an honest mix of line lengths and byte values.
    : > "$scratch/text"
    while [ "$(wc -c < "$scratch/text")" -lt 134217728 ]; do
        cat "$ROOT"/src/*.c >> "$scratch/text"
    done
    echo "## 128 MiB real source -> /dev/null, cat-compat -n (cooked + SIMD)" >> "$OUT"
    hyperfine -N --warmup 3 --export-markdown "$scratch/h.md" \
        "$MAT -n $scratch/text" "cat -n $scratch/text" >/dev/null 2>&1 || true
    cat "$scratch/h.md" >> "$OUT" 2>/dev/null
    echo >> "$OUT"

    # Decorated path: the render + highlight pipeline (gutter, grid, syntax
    # coloring). This is the path that competes with bat and the one LTO and
    # the render/highlight optimizations move — previously UNMEASURED, so
    # regressions here were invisible to the perf gate. --decorations=always
    # forces the frame even though stdout is a pipe under hyperfine.
    code="$scratch/code.c"
    : > "$code"
    i=0; while [ "$i" -lt 16 ]; do cat "$ROOT"/src/*.c >> "$code"; i=$((i+1)); done
    DECO="--decorations=always --style=full --color=always --paging=never"
    echo "## decorated source -> /dev/null (render + highlight)" >> "$OUT"
    if [ -n "$BAT" ]; then
        hyperfine -N --warmup 3 --export-markdown "$scratch/h.md" \
            "$MAT $DECO $code" \
            "bat --style=full --color=always --paging=never $code" \
            >/dev/null 2>&1 || true
    else
        hyperfine -N --warmup 3 --export-markdown "$scratch/h.md" \
            "$MAT $DECO $code" >/dev/null 2>&1 || true
    fi
    cat "$scratch/h.md" >> "$OUT" 2>/dev/null
    echo >> "$OUT"
    echo "_Data is random (never /dev/zero) for raw paths; real source for the" >> "$OUT"
    echo "cooked and decorated paths. \`-N\` runs without a shell where possible._" >> "$OUT"
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
