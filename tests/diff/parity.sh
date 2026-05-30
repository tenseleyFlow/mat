#!/bin/sh
# parity.sh — assert mat is byte-identical to the system cat.
#
# Sprint 00 covers the no-transform matrix. As cooked-path flags land
# (Sprint 02) add them to the FLAGS loop. Compares stdout AND exit status.
set -u

MAT=${MAT:-./mat}
fail=0
tmp=$(mktemp -d "${TMPDIR:-/tmp}/mat_parity.XXXXXX")
trap 'rm -rf "$tmp"' EXIT INT TERM

# --- fixtures (built deterministically; nothing binary committed) ---
printf ''                > "$tmp/empty"
printf 'single line\n'   > "$tmp/single"
printf 'a\nb\nc\n'       > "$tmp/multi"
printf 'no newline tail' > "$tmp/notail"
printf 'x\r\ny\r\n'      > "$tmp/crlf"
# every byte value 0..255, including NUL
i=0; : > "$tmp/all256"
while [ "$i" -lt 256 ]; do
    printf "\\$(printf '%03o' "$i")" >> "$tmp/all256"
    i=$((i + 1))
done
# 1 MiB of data (urandom if available, else zeros)
head -c 1048576 /dev/urandom > "$tmp/big" 2>/dev/null \
    || dd if=/dev/zero bs=1024 count=1024 > "$tmp/big" 2>/dev/null

ok=0
check() { # desc -- args...
    desc=$1; shift
    "$MAT" "$@" > "$tmp/m.out" 2>/dev/null; rm="$?"
    cat   "$@" > "$tmp/c.out" 2>/dev/null; rc="$?"
    if cmp -s "$tmp/m.out" "$tmp/c.out" && [ "$rm" = "$rc" ]; then
        ok=$((ok + 1))
    else
        echo "FAIL - $desc (exit mat=$rm cat=$rc)"; fail=1
    fi
}
check_stdin() { # desc infile -- args...
    desc=$1; infile=$2; shift 2
    "$MAT" "$@" < "$infile" > "$tmp/m.out" 2>/dev/null; rm="$?"
    cat   "$@" < "$infile" > "$tmp/c.out" 2>/dev/null; rc="$?"
    if cmp -s "$tmp/m.out" "$tmp/c.out" && [ "$rm" = "$rc" ]; then
        ok=$((ok + 1))
    else
        echo "FAIL - $desc (exit mat=$rm cat=$rc)"; fail=1
    fi
}

check "single file"          "$tmp/single"
check "empty file"           "$tmp/empty"
check "multi concat"         "$tmp/single" "$tmp/multi"
check "no trailing newline"  "$tmp/notail"
check "crlf"                 "$tmp/crlf"
check "all 256 byte values"  "$tmp/all256"
check "large 1MiB file"      "$tmp/big"
check "missing file exit"    "$tmp/does_not_exist"
check_stdin "stdin, no args" "$tmp/all256"
check_stdin "stdin via -"    "$tmp/multi" -

# file / stdin / file interleave
printf 'mid\n' | "$MAT" "$tmp/single" - "$tmp/multi" > "$tmp/m.out" 2>/dev/null
printf 'mid\n' | cat   "$tmp/single" - "$tmp/multi" > "$tmp/c.out" 2>/dev/null
if cmp -s "$tmp/m.out" "$tmp/c.out"; then ok=$((ok + 1))
else echo "FAIL - interleave a - b"; fail=1; fi

if [ "$fail" -eq 0 ]; then
    echo "parity: $ok/$ok cases byte-identical to cat"
fi
exit $fail
