#!/bin/sh
# run.sh — config-file + env precedence (behavioral).
#
# Asserts the resolved decoration style for each layer of the precedence chain
# by inspecting the first output line. Uses a private XDG dir so it never reads
# the developer's real config, and clears MAT_NO_CONFIG (which the orchestrator
# sets) because this test specifically exercises config loading.
set -u
unset MAT_NO_CONFIG

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
MAT=${MAT:-$ROOT/mat}
case "$MAT" in /*) ;; *) MAT="$(pwd)/$MAT" ;; esac

tmp=$(mktemp -d "${TMPDIR:-/tmp}/mat_cfg.XXXXXX")
trap 'rm -rf "$tmp"' EXIT INT TERM
mkdir -p "$tmp/cfg/mat"
printf -- '# test config\n--style=numbers\n--tabs=2\n' > "$tmp/cfg/mat/config"
printf 'hello\n' > "$tmp/f"

fail=0
# Run with the private config dir; emit the first output line.
firstline() { # extra args...
    XDG_CONFIG_HOME="$tmp/cfg" "$MAT" --decorations=always --color=never \
        --terminal-width=40 "$@" "$tmp/f" 2>/dev/null | head -1
}

# style=numbers -> a line-number gutter "   1 hello"
case "$(firstline)" in
    "   1 hello") ;;
    *) echo "FAIL: config --style not applied"; fail=1 ;;
esac

# CLI --style=grid overrides the config's numbers (grid rule starts with ─)
out=$(firstline --style=grid)
case "$out" in
    "$(printf '\342\224\200')"*) ;;
    *) echo "FAIL: CLI --style did not override config"; fail=1 ;;
esac

# MAT_STYLE env overrides the config file
out=$(XDG_CONFIG_HOME="$tmp/cfg" MAT_STYLE=grid "$MAT" --decorations=always \
    --color=never --terminal-width=40 "$tmp/f" 2>/dev/null | head -1)
case "$out" in
    "$(printf '\342\224\200')"*) ;;
    *) echo "FAIL: MAT_STYLE did not override config file"; fail=1 ;;
esac

# --no-config ignores the file -> default style (full frame, top ┬ rule)
out=$(XDG_CONFIG_HOME="$tmp/cfg" "$MAT" --no-config --decorations=always \
    --color=never --terminal-width=40 "$tmp/f" 2>/dev/null | head -1)
case "$out" in
    *"$(printf '\342\224\254')"*) ;; # contains ┬
    *) echo "FAIL: --no-config still read config"; fail=1 ;;
esac

# --config-file prints the resolved path
case "$(XDG_CONFIG_HOME="$tmp/cfg" "$MAT" --config-file)" in
    "$tmp/cfg/mat/config") ;;
    *) echo "FAIL: --config-file path wrong"; fail=1 ;;
esac

# MAT_OPTS sets decoration style (no config file)
out=$(MAT_NO_CONFIG= XDG_CONFIG_HOME="$tmp/empty" MAT_OPTS="--style=numbers" \
    "$MAT" --decorations=always --color=never --terminal-width=40 "$tmp/f" \
    2>/dev/null | head -1)
case "$out" in
    *"   1 hello"*) ;;
    *) echo "FAIL: MAT_OPTS --style=numbers not applied"; fail=1 ;;
esac

# BAT_OPTS fallback when MAT_OPTS is unset
out=$(MAT_NO_CONFIG= XDG_CONFIG_HOME="$tmp/empty" BAT_OPTS="--style=numbers" \
    "$MAT" --decorations=always --color=never --terminal-width=40 "$tmp/f" \
    2>/dev/null | head -1)
case "$out" in
    *"   1 hello"*) ;;
    *) echo "FAIL: BAT_OPTS fallback not applied"; fail=1 ;;
esac

# MAT_OPTS overrides BAT_OPTS
out=$(MAT_NO_CONFIG= XDG_CONFIG_HOME="$tmp/empty" \
    MAT_OPTS="--style=plain" BAT_OPTS="--style=numbers" \
    "$MAT" --decorations=always --color=never --terminal-width=40 "$tmp/f" \
    2>/dev/null | head -1)
case "$out" in
    "hello") ;;
    *) echo "FAIL: MAT_OPTS did not override BAT_OPTS"; fail=1 ;;
esac

# BAT_STYLE fallback when MAT_STYLE is unset
out=$(MAT_NO_CONFIG= XDG_CONFIG_HOME="$tmp/empty" BAT_STYLE=numbers \
    "$MAT" --decorations=always --color=never --terminal-width=40 "$tmp/f" \
    2>/dev/null | head -1)
case "$out" in
    *"   1 hello"*) ;;
    *) echo "FAIL: BAT_STYLE fallback not applied"; fail=1 ;;
esac

# MAT_TABS=8: verify wider tab expansion
printf '\thello\n' > "$tmp/tabfile"
out=$(MAT_NO_CONFIG= XDG_CONFIG_HOME="$tmp/empty" MAT_TABS=8 \
    "$MAT" --decorations=always --color=never --style=plain \
    --terminal-width=80 "$tmp/tabfile" 2>/dev/null | head -1)
case "$out" in
    "        hello") ;;
    *) echo "FAIL: MAT_TABS=8 not applied (got: '$out')"; fail=1 ;;
esac

[ "$fail" -eq 0 ] && echo "config: precedence OK"
exit $fail
