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

check() { # name -- compares captured out/err/rc against goldens
    name=$1
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

run() { # name -- command...
    name=$1; shift
    "$@" > "$scratch/$name.out" 2>"$scratch/$name.err"
    echo $? > "$scratch/$name.rc"
    check "$name"
}

# Like run, but feeds a file on stdin so decorated output shows a stable
# "File: STDIN" header instead of the random scratch path.
run_stdin() { # name infile -- command...
    name=$1; in=$2; shift 2
    "$@" < "$in" > "$scratch/$name.out" 2>"$scratch/$name.err"
    echo $? > "$scratch/$name.rc"
    check "$name"
}

# Like run_stdin, but pipes the input so the fd is non-seekable, exercising the
# streaming ring path (last-N without whole-file buffering) instead of mmap.
run_pipe() { # name infile -- command...
    name=$1; in=$2; shift 2
    cat "$in" | "$@" > "$scratch/$name.out" 2>"$scratch/$name.err"
    echo $? > "$scratch/$name.rc"
    check "$name"
}

# Version includes a commit hash that changes every commit, so check the
# format ("mat X.Y.Z (hash)") rather than an exact golden.
ver=$("$MAT" --version 2>&1)
if echo "$ver" | grep -qE '^mat [0-9]+\.[0-9]+\.[0-9]+ \([0-9a-z]+\)$'; then
    echo "ok   - version"
else
    echo "FAIL - version: got '$ver'"; fail=1
fi

run help     "$MAT" --help
run badopt   "$MAT" -Z
run badlong  "$MAT" --nope

# Line ranges (-r): no `cat` analogue, so assert exact selected line sets.
awk 'BEGIN { for (i = 1; i <= 20; i++) print i }' > "$scratch/lines.txt"
run range_nm     "$MAT" -r 3:5    "$scratch/lines.txt"
run range_tom    "$MAT" -r :3     "$scratch/lines.txt"
run range_ton    "$MAT" -r 18:    "$scratch/lines.txt"
run range_lastn  "$MAT" -r -2:    "$scratch/lines.txt"
run range_plus   "$MAT" -r 5:+2   "$scratch/lines.txt"
run range_ctx    "$MAT" -r 10::1  "$scratch/lines.txt"
run range_multi  "$MAT" -r 2:3 -r 8:9 "$scratch/lines.txt"
run range_single "$MAT" -r 7      "$scratch/lines.txt"
run range_badarg "$MAT" -r nope   "$scratch/lines.txt"

# Decorated output (--pretty forces the frame even when piped). Fed on stdin so
# the header is a stable "File: STDIN" rather than the random scratch path.
run_stdin pretty_plain "$scratch/lines.txt" "$MAT" --pretty --color=never
run_stdin range_pretty "$scratch/lines.txt" "$MAT" --pretty --color=never -r 3:5
run_stdin range_snip   "$scratch/lines.txt" "$MAT" --pretty --color=never -r 2:3 -r 8:9
# Highlighted lines (-H), captured with color on to assert the SGR spans.
run_stdin hl_stream    "$scratch/lines.txt" "$MAT" --pretty --color=always -H 3
run_stdin hl_in_range  "$scratch/lines.txt" "$MAT" --pretty --color=always -r 2:6 -H 4

# --squeeze-limit caps consecutive blanks (default 1 == cat -s).
printf 'a\n\n\n\n\nb\n\n\n\nc\n' > "$scratch/blanks.txt"
run squeeze_def  "$MAT" -s "$scratch/blanks.txt"
run squeeze_two  "$MAT" -s --squeeze-limit 2 "$scratch/blanks.txt"
run squeeze_zero "$MAT" -s --squeeze-limit 0 "$scratch/blanks.txt"

# Streaming ring path (piped = non-seekable): last-N and mixed ranges must
# match the seekable result without buffering the whole stream.
run_pipe range_lastn_pipe "$scratch/lines.txt" "$MAT" -r -3:
run_pipe range_mix_pipe   "$scratch/lines.txt" "$MAT" -r 1:3 -r -2:
run_pipe range_bound_pipe "$scratch/lines.txt" "$MAT" -r 2:4

# Encoding (Sprint 07): UTF-16 decoded to UTF-8, UTF-8 BOM stripped, binary
# files skipped-with-notice (decorated) or shown raw with --binary=as-text.
printf '\377\376\150\000\151\000\012\000' > "$scratch/u16le.bin" # BOM + "hi\n"
run enc_utf16le "$MAT" -r 1: "$scratch/u16le.bin"
printf '\357\273\277\150\151\012' > "$scratch/u8bom.txt" # BOM + "hi\n"
run enc_u8bom "$MAT" -r 1: "$scratch/u8bom.txt"
printf '\141\142\000\143\012\144\145\146\012' > "$scratch/binary.bin" # NUL inside
run_stdin enc_binary_notice "$scratch/binary.bin" "$MAT" --pretty --color=never -r 1:
run_stdin enc_binary_astext "$scratch/binary.bin" "$MAT" \
    --pretty --color=never --binary=as-text -r 1:

# --strip-ansi: auto keeps escapes in plain output but strips under decorations;
# always/never force it.
printf '\033[31mred\033[0m line\nplain line\n' > "$scratch/ansi.txt"
run ansi_keep_plain   "$MAT" -r 1: "$scratch/ansi.txt"
run ansi_strip_always "$MAT" -r 1: --strip-ansi=always "$scratch/ansi.txt"
run_stdin ansi_deco_strip "$scratch/ansi.txt" "$MAT" --pretty --color=never -r 1:
run_stdin ansi_deco_keep "$scratch/ansi.txt" "$MAT" \
    --pretty --color=never --strip-ansi=never -r 1:

# strip-ansi on the interactive path (no -r, tests interactive.c wiring)
"$MAT" --pretty --color=never --strip-ansi=always "$scratch/ansi.txt" \
    > "$scratch/ansi_interactive.out" 2>/dev/null
if grep -q $'\033' "$scratch/ansi_interactive.out"; then
    echo "FAIL - ansi_interactive_strip (ANSI codes still present)"; fail=1
else
    echo "ok   - ansi_interactive_strip"
fi

# Syntax detection (--detect-syntax). Fed on stdin with --file-name for a stable
# display name; covers extension, -l override, -m glob, and shebang order.
printf 'content\n' > "$scratch/detect.in"
run_stdin detect_ext "$scratch/detect.in" "$MAT" --file-name foo.c --detect-syntax
run_stdin detect_lang "$scratch/detect.in" "$MAT" \
    -l Rust --file-name foo.c --detect-syntax
run_stdin detect_map "$scratch/detect.in" "$MAT" \
    -m '*.q:Qlang' --file-name a.q --detect-syntax
printf '#!/usr/bin/env python3\n' > "$scratch/shebang.in"
run_stdin detect_shebang "$scratch/shebang.in" "$MAT" --detect-syntax

# Parallel output: multi-file decorated output must be stable (same output on
# repeated runs) and contain each file's content in input order.
printf 'int x;\n' > "$scratch/p1.c"
printf 'int y;\n' > "$scratch/p2.c"
printf 'int z;\n' > "$scratch/p3.c"
"$MAT" --pretty --color=never "$scratch/p1.c" "$scratch/p2.c" "$scratch/p3.c" \
    > "$scratch/par_run1.out" 2>/dev/null
"$MAT" --pretty --color=never "$scratch/p1.c" "$scratch/p2.c" "$scratch/p3.c" \
    > "$scratch/par_run2.out" 2>/dev/null
par_ok=1
if [ ! -s "$scratch/par_run1.out" ]; then
    echo "FAIL - parallel_empty (no output)"; par_ok=0; fail=1
fi
if ! cmp -s "$scratch/par_run1.out" "$scratch/par_run2.out"; then
    echo "FAIL - parallel_stable (two runs differ)"; par_ok=0; fail=1
fi
if ! grep -q "int x;" "$scratch/par_run1.out" || \
   ! grep -q "int y;" "$scratch/par_run1.out" || \
   ! grep -q "int z;" "$scratch/par_run1.out"; then
    echo "FAIL - parallel_content (missing file content)"; par_ok=0; fail=1
fi
# Verify ordering: x before y before z
xline=$(grep -n "int x;" "$scratch/par_run1.out" | head -1 | cut -d: -f1)
yline=$(grep -n "int y;" "$scratch/par_run1.out" | head -1 | cut -d: -f1)
zline=$(grep -n "int z;" "$scratch/par_run1.out" | head -1 | cut -d: -f1)
if [ "$xline" -lt "$yline" ] && [ "$yline" -lt "$zline" ]; then
    : # order correct
else
    echo "FAIL - parallel_order (files not in input order)"; par_ok=0; fail=1
fi
[ "$par_ok" -eq 1 ] && echo "ok   - parallel"

# Streaming ring edge cases (piped = non-seekable).
printf '' | "$MAT" -r -3: > "$scratch/ring_empty.out" 2>/dev/null
ring_empty_rc=$?
if [ ! -s "$scratch/ring_empty.out" ] && [ "$ring_empty_rc" -eq 0 ]; then
    echo "ok   - ring_empty"
else
    echo "FAIL - ring_empty (rc=$ring_empty_rc, size=$(wc -c < "$scratch/ring_empty.out"))"; fail=1
fi

echo "one" | "$MAT" -r -1: > "$scratch/ring_single.out" 2>/dev/null
if [ "$(cat "$scratch/ring_single.out")" = "one" ]; then
    echo "ok   - ring_single"
else
    echo "FAIL - ring_single"; fail=1
fi

printf 'a\nb\n' | "$MAT" -r -5: > "$scratch/ring_short.out" 2>/dev/null
expected=$(printf 'a\nb\n')
if [ "$(cat "$scratch/ring_short.out")" = "$expected" ]; then
    echo "ok   - ring_short"
else
    echo "FAIL - ring_short"; fail=1
fi

awk 'BEGIN{for(i=1;i<=100;i++)print i}' | "$MAT" -r -3: \
    > "$scratch/ring_wrap.out" 2>/dev/null
expected=$(printf '98\n99\n100\n')
if [ "$(cat "$scratch/ring_wrap.out")" = "$expected" ]; then
    echo "ok   - ring_wrap"
else
    echo "FAIL - ring_wrap"; fail=1
fi

# Mixed absolute + last-N on a stream.
awk 'BEGIN{for(i=1;i<=10;i++)print i}' | "$MAT" -r 1:2 -r -1: \
    > "$scratch/ring_mixed.out" 2>/dev/null
expected=$(printf '1\n2\n10\n')
if [ "$(cat "$scratch/ring_mixed.out")" = "$expected" ]; then
    echo "ok   - ring_mixed"
else
    echo "FAIL - ring_mixed"; fail=1
fi

# T2: cooked path squeeze across cat-concat boundary
f1="$scratch/sq1.txt"
f2="$scratch/sq2.txt"
printf 'a\n\n\n\n' > "$f1"
printf '\n\n\nb\n' > "$f2"
got=$("$MAT" -s "$f1" "$f2")
expected=$(printf 'a\n\nb\n')
if [ "$got" = "$expected" ]; then
    echo "ok   - cooked_squeeze_boundary"
else
    echo "FAIL - cooked_squeeze_boundary"; fail=1
fi

# T2: cooked -n across buffer boundary (line numbers continuous)
dd if=/dev/zero bs=131072 count=1 2>/dev/null | tr '\0' 'x' > "$scratch/bigline.txt"
printf '\nsecond\n' >> "$scratch/bigline.txt"
line2=$("$MAT" -n "$scratch/bigline.txt" | sed -n '2s/^[[:space:]]*//;2p')
case "$line2" in
    2*second) echo "ok   - cooked_boundary_linenum" ;;
    *)        echo "FAIL - cooked_boundary_linenum (got: $line2)"; fail=1 ;;
esac

# T4: --diff on a file not in a git repo — no crash
echo "hello" > "$scratch/notgit.txt"
"$MAT" --pretty --diff --color=never "$scratch/notgit.txt" > /dev/null 2>&1
rc=$?
if [ "$rc" -eq 0 ]; then
    echo "ok   - diff_no_git_repo"
else
    echo "FAIL - diff_no_git_repo (exit $rc)"; fail=1
fi

# T6: parallel error propagation — one missing file
printf 'aaa\n' > "$scratch/p_ok1.txt"
printf 'bbb\n' > "$scratch/p_ok2.txt"
out=$("$MAT" --pretty --color=never "$scratch/p_ok1.txt" /nonexistent "$scratch/p_ok2.txt" 2>/dev/null)
rc=$?
if [ "$rc" -ne 0 ] && echo "$out" | grep -q 'aaa' && echo "$out" | grep -q 'bbb'; then
    echo "ok   - parallel_error_propagation"
else
    echo "FAIL - parallel_error_propagation (rc=$rc)"; fail=1
fi

# T6: 10+ files all appear in order
for n in $(seq 1 12); do
    printf 'file%d\n' "$n" > "$scratch/pf_$n.txt"
done
out=$("$MAT" --pretty --color=never "$scratch"/pf_*.txt 2>/dev/null)
allfound=true
for n in $(seq 1 12); do
    echo "$out" | grep -q "file$n" || allfound=false
done
if $allfound; then
    echo "ok   - parallel_12_files"
else
    echo "FAIL - parallel_12_files"; fail=1
fi

# T3: SIGPIPE — mat exits cleanly when pipe reader closes early
dd if=/dev/urandom bs=1M count=1 2>/dev/null > "$scratch/bigrand.bin"
"$MAT" "$scratch/bigrand.bin" 2>/dev/null | head -1 > /dev/null
rc=$?
if [ "$rc" -eq 0 ] || [ "$rc" -eq 141 ]; then
    echo "ok   - sigpipe_head"
else
    echo "FAIL - sigpipe_head (exit $rc)"; fail=1
fi

# T6: diff marker golden — controlled git repo
if command -v git > /dev/null 2>&1; then
    gd="$scratch/gitfix"
    mkdir -p "$gd"
    (
        cd "$gd"
        git init -q
        git config user.email "test@test"
        git config user.name "test"
        git config commit.gpgsign false
        printf 'line1\nline2\nline3\nline4\n' > f.txt
        git add f.txt && git commit -q -m init
        printf 'line1\nchanged\nline3\nline4\nnew\n' > f.txt
    )
    out=$(cd "$gd" && "$MAT" --pretty --diff --color=never --no-paging f.txt 2>/dev/null)
    if echo "$out" | grep -q '~' && echo "$out" | grep -q '+'; then
        echo "ok   - diff_marker_golden"
    else
        echo "FAIL - diff_marker_golden"; fail=1
    fi
else
    echo "skip - diff_marker_golden (git not installed)"
fi

# T3: --diagnostic output contains correct counts
diag=$("$MAT" --diagnostic 2>/dev/null)
if echo "$diag" | grep -q 'languages: 130' && echo "$diag" | grep -q 'themes: 45'; then
    echo "ok   - diagnostic_counts"
else
    echo "FAIL - diagnostic_counts"; fail=1
fi

# T5: /dev/null input — empty output, exit 0
devnull_out=$("$MAT" /dev/null 2>/dev/null)
devnull_rc=$?
if [ "$devnull_rc" -eq 0 ] && [ -z "$devnull_out" ]; then
    echo "ok   - devnull_input"
else
    echo "FAIL - devnull_input (rc=$devnull_rc, len=${#devnull_out})"; fail=1
fi

# T5: directory as argument — non-zero exit
"$MAT" /tmp > /dev/null 2>&1
dir_rc=$?
if [ "$dir_rc" -ne 0 ]; then
    echo "ok   - dir_as_input"
else
    echo "FAIL - dir_as_input (exit 0)"; fail=1
fi

# T5: permission denied (skip if root)
if [ "$(id -u)" != "0" ]; then
    noperm="$scratch/noperm.txt"
    printf 'secret\n' > "$noperm"
    chmod 000 "$noperm"
    "$MAT" "$noperm" > /dev/null 2>&1
    perm_rc=$?
    chmod 644 "$noperm"
    if [ "$perm_rc" -ne 0 ]; then
        echo "ok   - permission_denied"
    else
        echo "FAIL - permission_denied (exit 0)"; fail=1
    fi
else
    echo "skip - permission_denied (running as root)"
fi

# T3: wrapping + highlighting combined
printf 'int very_long_variable_name_that_exceeds_forty_columns = some_function(arg1, arg2, arg3);\n' \
    > "$scratch/longline.c"
wrapout=$("$MAT" --pretty --color=always --terminal-width=40 --wrap=character \
    --no-paging "$scratch/longline.c" 2>/dev/null)
wraplines=$(echo "$wrapout" | wc -l)
if [ "$wraplines" -gt 3 ] && echo "$wrapout" | grep -q "$(printf '\033')" ; then
    echo "ok   - wrap_plus_highlight"
else
    echo "FAIL - wrap_plus_highlight (lines=$wraplines)"; fail=1
fi

# T4: gutter >9999 lines
seq 1 10001 > "$scratch/biglines.txt"
lastgutter=$("$MAT" --pretty --color=never --no-paging "$scratch/biglines.txt" \
    2>/dev/null | tail -2 | head -1)
if echo "$lastgutter" | grep -q '10001'; then
    echo "ok   - gutter_10001"
else
    echo "FAIL - gutter_10001 (got: $lastgutter)"; fail=1
fi

[ "$update" -eq 1 ] && echo "integration: goldens updated"
exit $fail
