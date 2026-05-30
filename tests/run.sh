#!/bin/sh
# run.sh — Sprint 00 test orchestrator: unit (Unity) + integration + parity.
set -u
export MAT_NO_CONFIG=1  # hermetic: ignore any developer config

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1
CC=${CC:-cc}
UNITY=tests/vendor/unity
fail=0

[ -f config.mk ] || ./configure >/dev/null 2>&1
# Reuse configure's feature-test macro so unit tests see POSIX symbols too.
FEATURE=$(sed -n 's/^FEATURE_CFLAGS = //p' config.mk 2>/dev/null)
TCFLAGS="-std=c11 -O2 $FEATURE -Isrc -Ilib/paige/include -Ilib/paige/src -I$UNITY"
# matpager.c (pulled in below) links the paige submodule.
PAIGE_SRC="lib/paige/src/term.c lib/paige/src/pager.c"

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
    if $CC $TCFLAGS -o "$bin" "$t" "$UNITY/unity.c" $SRC_NOMAIN $PAIGE_SRC; then
        "$bin" || fail=1
    else
        echo "compile $name FAILED"; fail=1
    fi
done

echo "== integration =="
MAT="$MAT" sh tests/integration/run.sh || fail=1

echo "== cooked goldens =="
MAT="$MAT" sh tests/cooked/run.sh || fail=1

echo "== decoration goldens =="
MAT="$MAT" sh tests/decorations/run.sh || fail=1

echo "== config precedence =="
MAT="$MAT" sh tests/config/run.sh || fail=1

echo "== pager (pty) =="
MAT="$MAT" sh tests/pager/run.sh || fail=1

echo "== parity =="
MAT="$MAT" sh tests/diff/parity.sh || fail=1

if [ "$fail" -eq 0 ]; then
    echo "ALL TESTS PASSED"
else
    echo "SOME TESTS FAILED"
fi
exit $fail
