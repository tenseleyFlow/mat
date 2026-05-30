#!/bin/sh
# run.sh — decoration-frame golden tests.
#
# mat's frame output is deterministic (fixed box-drawing bytes, fixed width via
# --terminal-width), so these goldens are platform-independent. Validated by
# hand against bat 0.25 at authoring time; the goldens then assert mat's own
# output. Regenerate with: sh tests/decorations/run.sh --update
set -u
export MAT_NO_CONFIG=1  # hermetic: ignore any developer config

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
MAT=${MAT:-$ROOT/mat}
# Absolutize MAT so it survives the cd into the fixture dir below.
case "$MAT" in
    /*) ;;
    *) MAT="$(pwd)/$MAT" ;;
esac
G="$ROOT/tests/decorations/golden"
update=0
[ "${1:-}" = "--update" ] && update=1
mkdir -p "$G"

scratch=$(mktemp -d "${TMPDIR:-/tmp}/mat_deco.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
F="$scratch/fix"
mkdir -p "$F"
printf 'int main(void) {\n    return 0;\n}\n'  > "$F/src"
printf 'one\ntwo\nthree\nfour\nfive\n'         > "$F/lines"
printf 'no trailing newline'                   > "$F/notail"
printf ''                                      > "$F/empty"
printf 'tab\there\nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx end\nword1 word2 word3 word4 word5 word6\n' > "$F/wrap"

# Run from the fixture dir so the "File:" header shows a stable basename, not
# the random temp path.
cd "$F" || exit 1

COMMON="--color=never --decorations=always --terminal-width=72"
STYLES="plain numbers numbers,grid grid header default full"
fail=0
ok=0

check() { # goldenname -- file to compare
    name=$1
    g="$G/$name"
    if [ "$update" -eq 1 ]; then
        cp "$scratch/out" "$g"
    elif ! cmp -s "$scratch/out" "$g"; then
        echo "FAIL decoration golden: $name"
        diff "$g" "$scratch/out" 2>/dev/null | head -16
        fail=1
    else
        ok=$((ok + 1))
    fi
}

for fx in src lines notail empty; do
    : > "$scratch/out"
    for st in $STYLES; do
        printf '### --style=%s\n' "$st" >> "$scratch/out"
        # shellcheck disable=SC2086
        "$MAT" $COMMON --style="$st" "$fx" >> "$scratch/out" 2>/dev/null
        printf '\n' >> "$scratch/out"
    done
    check "$fx"
done

# multi-file framing with rule separators
# shellcheck disable=SC2086
"$MAT" $COMMON --style=full,rule src lines > "$scratch/out" 2>/dev/null
check "multi"

# color=always: lock the ANSI gutter codes
"$MAT" --color=always --decorations=always --terminal-width=72 \
    --style=numbers,grid lines > "$scratch/out" 2>/dev/null
check "color"

# wrapping + tab expansion at a narrow width, across wrap modes
: > "$scratch/out"
for wm in auto character word never; do
    printf '### --wrap=%s\n' "$wm" >> "$scratch/out"
    "$MAT" --color=never --decorations=always --terminal-width=30 \
        --style=numbers,grid --wrap="$wm" wrap >> "$scratch/out" 2>/dev/null
    printf '\n' >> "$scratch/out"
done
check "wrap"

if [ "$update" -eq 1 ]; then
    echo "decoration goldens updated"
    exit 0
fi
[ "$fail" -eq 0 ] && echo "decorations: $ok/$ok goldens match"
exit $fail
