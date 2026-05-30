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
# Progress to stderr so a hanging case is visible in CI logs (the last line
# printed is the culprit). Unbuffered: each echo is its own write().
say() { echo "[parity] $*" >&2; }

check() { # desc -- args...
    desc=$1; shift
    say "$desc"
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
    say "$desc"
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
say "interleave a - b"
printf 'mid\n' | "$MAT" "$tmp/single" - "$tmp/multi" > "$tmp/m.out" 2>/dev/null
printf 'mid\n' | cat   "$tmp/single" - "$tmp/multi" > "$tmp/c.out" 2>/dev/null
if cmp -s "$tmp/m.out" "$tmp/c.out"; then ok=$((ok + 1))
else echo "FAIL - interleave a - b"; fail=1; fi

# --- pipe output: exercises the splice path (stdout is a pipe) ---
check_pipe() { # desc -- args...
    desc=$1; shift
    say "$desc"
    "$MAT" "$@" | cat > "$tmp/m.out" 2>/dev/null
    cat   "$@" | cat > "$tmp/c.out" 2>/dev/null
    if cmp -s "$tmp/m.out" "$tmp/c.out"; then ok=$((ok + 1))
    else echo "FAIL - $desc"; fail=1; fi
}
check_pipe "pipe out: large file (splice)"      "$tmp/big"
check_pipe "pipe out: small file (rw fallback)" "$tmp/single"
check_pipe "pipe out: all 256 bytes"            "$tmp/all256"
check_pipe "pipe out: empty file"               "$tmp/empty"

# pipe in -> pipe out: both ends pipes, splice can't, falls back to read/write
say "pipe in -> pipe out"
cat "$tmp/big" | "$MAT" | cat > "$tmp/m.out" 2>/dev/null
cat "$tmp/big" | cat    | cat > "$tmp/c.out" 2>/dev/null
if cmp -s "$tmp/m.out" "$tmp/c.out"; then ok=$((ok + 1))
else echo "FAIL - pipe in -> pipe out"; fail=1; fi

# self-overwrite: mat f > f  (shell truncates f first; both produce empty, exit 0)
say "self overwrite > f"
cp "$tmp/multi" "$tmp/self_m"; cp "$tmp/multi" "$tmp/self_c"
"$MAT" "$tmp/self_m" > "$tmp/self_m" 2>/dev/null; rm="$?"
cat   "$tmp/self_c" > "$tmp/self_c" 2>/dev/null; rc="$?"
if cmp -s "$tmp/self_m" "$tmp/self_c" && [ "$rm" = "$rc" ]; then ok=$((ok + 1))
else echo "FAIL - self overwrite '> f' (exit mat=$rm cat=$rc)"; fail=1; fi

# self-append: mat f >> f  (SAME_INODE guard must refuse, like cat: exit 1, unchanged)
say "self append >> f"
cp "$tmp/multi" "$tmp/app_m"; cp "$tmp/multi" "$tmp/app_c"
"$MAT" "$tmp/app_m" >> "$tmp/app_m" 2>/dev/null; rm="$?"
cat   "$tmp/app_c" >> "$tmp/app_c" 2>/dev/null; rc="$?"
if cmp -s "$tmp/app_m" "$tmp/app_c" && [ "$rm" = "$rc" ]; then ok=$((ok + 1))
else echo "FAIL - self append '>> f' (exit mat=$rm cat=$rc)"; fail=1; fi

if [ "$fail" -eq 0 ]; then
    echo "parity: $ok/$ok cases byte-identical to cat"
fi
exit $fail
