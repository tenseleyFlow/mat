#!/bin/sh
# run.sh — drive mat's bespoke pager (paige) through a pseudo-terminal.
set -u
export MAT_NO_CONFIG=1

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1
MAT=${MAT:-$ROOT/mat}
case "$MAT" in /*) ;; *) MAT="$(pwd)/$MAT" ;; esac
CC=${CC:-cc}

mkdir -p tests/build

# Build a pty test (with -lutil for forkpty, falling back without it).
build_test() {
    src=$1
    out=$2
    if $CC -std=c11 -O2 -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE \
        "$src" -lutil -o "$out" 2>/dev/null; then
        return 0
    elif $CC -std=c11 -O2 -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE \
        "$src" -o "$out"; then
        return 0
    fi
    echo "could not build $src"
    exit 1
}

build_test tests/pager/pty_test.c tests/build/mat_pty
build_test tests/pager/hl_test.c tests/build/mat_hl

rc=0
tests/build/mat_pty "$MAT" || rc=1
tests/build/mat_hl "$MAT" || rc=1
exit "$rc"
