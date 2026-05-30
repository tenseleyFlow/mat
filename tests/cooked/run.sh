#!/bin/sh
# run.sh — cooked-path golden tests.
#
# mat's transformed output is deterministic and byte-oriented (no locale), so
# these goldens are platform-independent and serve as the PRIMARY oracle for the
# cooked path. (Differential-vs-cat is unreliable here: GNU cat's behavior
# differs across versions — e.g. -E on CRLF changed between coreutils 8 and 9 —
# and BSD cat differs more.) mat's contract follows coreutils 9.x semantics.
#
# Regenerate goldens with: sh tests/cooked/run.sh --update
set -u
export MAT_NO_CONFIG=1  # hermetic: ignore any developer config

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
MAT=${MAT:-$ROOT/mat}
G="$ROOT/tests/cooked/golden"
update=0
[ "${1:-}" = "--update" ] && update=1
mkdir -p "$G"

scratch=$(mktemp -d "${TMPDIR:-/tmp}/mat_cooked.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM
F="$scratch/fix"
mkdir -p "$F"

# Deterministic fixtures (regenerated identically every run).
printf 'hello\nworld\n\n\n\nfoo\nbar\n'  > "$F/text"
printf 'a\tb\tc\n\tindented line\n'      > "$F/tabs"
printf 'l1\r\nl2\r\nbare\rcr\n'          > "$F/crlf"
printf 'no newline at end'               > "$F/notail"
printf 'x\n\n\n\n\n\n\ny\n'              > "$F/blanks"
i=0; : > "$F/bytes"
while [ "$i" -lt 256 ]; do
    printf "\\$(printf '%03o' "$i")" >> "$F/bytes"
    i=$((i + 1))
done

FLAGS="n b s e t v A E T ns bs sv vet nve sA Anb"
fail=0
ok=0
for fx in "$F"/text "$F"/tabs "$F"/crlf "$F"/notail "$F"/blanks "$F"/bytes; do
    name=$(basename "$fx")
    out="$scratch/$name.combined"
    : > "$out"
    for fl in $FLAGS; do
        printf '### mat -%s\n' "$fl" >> "$out"
        "$MAT" "-$fl" "$fx" >> "$out" 2>/dev/null
        printf '\n' >> "$out"
    done
    g="$G/$name"
    if [ "$update" -eq 1 ]; then
        cp "$out" "$g"
    elif ! cmp -s "$out" "$g"; then
        echo "FAIL cooked golden: $name"
        diff "$g" "$out" 2>/dev/null | head -20
        fail=1
    else
        ok=$((ok + 1))
    fi
done

if [ "$update" -eq 1 ]; then
    echo "cooked goldens updated"
    exit 0
fi
[ "$fail" -eq 0 ] && echo "cooked: $ok/$ok fixture goldens match"
exit $fail
