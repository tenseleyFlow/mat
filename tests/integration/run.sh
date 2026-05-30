#!/bin/sh
# run.sh — golden-file integration tests for output that has no `cat` analogue
# (help, version, usage errors). Run with --update to regenerate goldens.
set -u
export MAT_NO_CONFIG=1  # hermetic: ignore any developer config

MAT=${MAT:-./mat}
G=tests/integration/golden
scratch=$(mktemp -d "${TMPDIR:-/tmp}/mat_it.XXXXXX")
trap 'rm -rf "$scratch"' EXIT INT TERM

update=0
[ "${1:-}" = "--update" ] && update=1
mkdir -p "$G"
fail=0

# Invoke the binary under a stable name "mat" so progname in diagnostics is
# constant regardless of the real binary (mat, mat-asan, ...). argv[0] basename
# is what mat prints, so the goldens stay valid across builds.
case "$MAT" in
    /*) abs=$MAT ;;
    *)  abs=$PWD/$MAT ;;
esac
ln -sf "$abs" "$scratch/mat"
MAT="$scratch/mat"

run() { # name -- command...
    name=$1; shift
    "$@" > "$scratch/$name.out" 2>"$scratch/$name.err"
    echo $? > "$scratch/$name.rc"
    cfail=0
    for ext in out err rc; do
        af="$scratch/$name.$ext"; gf="$G/$name.$ext"
        if [ "$update" -eq 1 ]; then
            cp "$af" "$gf"
        elif ! cmp -s "$af" "$gf"; then
            echo "FAIL - $name.$ext"; cfail=1; fail=1
        fi
    done
    [ "$update" -eq 0 ] && [ "$cfail" -eq 0 ] && echo "ok   - $name"
}

run version  "$MAT" --version
run help     "$MAT" --help
run badopt   "$MAT" -Z
run badlong  "$MAT" --nope

[ "$update" -eq 1 ] && echo "integration: goldens updated"
exit $fail
