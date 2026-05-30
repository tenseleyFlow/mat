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
if $CC -std=c11 -O2 -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE \
    tests/pager/pty_test.c -lutil -o tests/build/mat_pty 2>/dev/null; then
    :
elif $CC -std=c11 -O2 -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE \
    tests/pager/pty_test.c -o tests/build/mat_pty; then
    :
else
    echo "could not build mat pager pty_test"; exit 1
fi

tests/build/mat_pty "$MAT"
