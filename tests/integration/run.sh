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

run version  "$MAT" --version
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

[ "$update" -eq 1 ] && echo "integration: goldens updated"
exit $fail
