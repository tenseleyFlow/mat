#!/bin/sh
# run.sh — Sprint 00 test orchestrator: unit (Unity) + integration + parity.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
CC=${CC:-cc}
UNITY=tests/vendor/unity
TCFLAGS="-std=c11 -O2 -Isrc -I$UNITY"
fail=0

[ -f config.mk ] || ./configure >/dev/null 2>&1

echo "== build =="
make >/dev/null || { echo "build failed"; exit 1; }
MAT="$ROOT/mat"

echo "== unit =="
mkdir -p tests/build
SRC_NOMAIN=$(ls src/*.c | grep -v '/main\.c$')
for t in tests/unit/test_*.c; do
    name=$(basename "$t" .c)
    bin="tests/build/$name"
    # shellcheck disable=SC2086
    if $CC $TCFLAGS -o "$bin" "$t" "$UNITY/unity.c" $SRC_NOMAIN; then
        "$bin" || fail=1
    else
        echo "compile $name FAILED"; fail=1
    fi
done

echo "== integration =="
MAT="$MAT" sh tests/integration/run.sh || fail=1

echo "== parity =="
MAT="$MAT" sh tests/diff/parity.sh || fail=1

if [ "$fail" -eq 0 ]; then
    echo "ALL TESTS PASSED"
else
    echo "SOME TESTS FAILED"
fi
exit $fail
