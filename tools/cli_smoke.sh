#!/usr/bin/env bash
#
# End-to-end smoke test of the jt* binaries — the `cli` stage of tools/ci.sh.
#
#   tools/cli_smoke.sh [bin-dir]        # default: ./build
#
# The happy-path expectations are the examples in README.md, which is the authoritative
# verb/flag reference: if one of them stops matching, either the code or the README is
# wrong, and this is where that gets decided. Error paths assert the contract instead of
# a full message — non-zero exit, and an "Error at <path>: ..." line on stderr — except
# the two the README spells out verbatim, which are compared exactly.
#
# Exit status: 0 when every check passes, 1 otherwise.

set -u

BIN=${1:-build}
if ! BIN=$(cd "$BIN" 2>/dev/null && pwd); then
    printf 'cli smoke: no such build directory: %s\n' "${1:-build}" >&2
    exit 2
fi

ok=0
failed=0

check() { # <name> <expected> <actual>
    if [ "$2" = "$3" ]; then
        printf '  ok   %s\n' "$1"
        ok=$((ok + 1))
    else
        printf '  FAIL %s\n         expected: %s\n         actual:   %s\n' "$1" "$2" "$3"
        failed=$((failed + 1))
    fi
}

check_err() { # <name> <stdin> <command...>
    local name="$1" input="$2"
    shift 2
    local err rc
    err=$(printf '%s' "$input" | "$@" 2>&1 >/dev/null)
    rc=$?
    if [ "$rc" -ne 0 ] && [ "${err#Error at }" != "$err" ]; then
        printf '  ok   %s (exit %s)\n' "$name" "$rc"
        ok=$((ok + 1))
    else
        printf '  FAIL %s\n         wanted a non-zero exit and an "Error at ..." line, got exit %s: %s\n' \
            "$name" "$rc" "$err"
        failed=$((failed + 1))
    fi
}

check_err_exact() { # <name> <stdin> <expected stderr> <command...>
    local name="$1" input="$2" want="$3"
    shift 3
    local err
    err=$(printf '%s' "$input" | "$@" 2>&1 >/dev/null)
    check "$name (stderr)" "$want" "$err"
}

check_stdout() { # <name> <expected> <stdin> <command...>
    local name="$1" want="$2" input="$3"
    shift 3
    local got
    got=$(printf '%s' "$input" | "$@")
    check "$name" "$want" "$got"
}

printf 'cli smoke: %s\n' "$BIN"

# --- the README examples -----------------------------------------------------
check "build an object (README)" \
    '{"age":48,"name":"harri"}' \
    "$("$BIN/jtNew" | "$BIN/jtSet" /name '"harri"' | "$BIN/jtSet" /age 48)"

check "nested via -p (README)" \
    '{"key1":{"key2":{"key3":"value"}}}' \
    "$("$BIN/jtNew" | "$BIN/jtSet" /key1/key2/key3 '"value"' -p)"

check_stdout "append with the /- sentinel (README)" '{"items":["a","b","c"]}' \
    '{"items":["a","b"]}' "$BIN/jtSet" /items/- '"c"'

check_stdout "move a value (README)" '{"name":"harri","user":{}}' \
    '{"user":{"name":"harri"}}' "$BIN/jtMove" /user/name /name

check_stdout "zip two arrays into an object (README)" '{"age":48,"name":"harri"}' \
    '{"keys":["name","age"],"vals":["harri",48]}' "$BIN/jtZip" /keys /vals

check_stdout "len writes a count, document intact (README)" '{"count":3,"items":[1,2,3]}' \
    '{"items":[1,2,3]}' "$BIN/jtLen" /items /count

check_stdout "keys of an object (README)" '["a","b"]' \
    '{"a":1,"b":2}' "$BIN/jtKeys"

check "keys -> values -> zip round trip (README)" '{"a":1,"b":2}' \
    "$(printf '%s' '{"o":{"a":1,"b":2}}' \
        | "$BIN/jtKeys" /o /ks | "$BIN/jtValues" /o /vs | "$BIN/jtZip" /ks /vs)"

# --- composed chains: the reason the tools exist -----------------------------
check "filter -> sort -> select, the README chain" \
    '{"users":[{"age":51,"name":"bob"},{"age":48,"name":"harri"}]}' \
    "$(printf '%s' '{"users":[{"name":"ada","age":36},{"name":"harri","age":48},{"name":"bob","age":51}]}' \
        | "$BIN/jtFilter" /users age --gt 40 | "$BIN/jtSort" /users name | "$BIN/jtSelect" /users)"

check "filter -> sort -> len -> get, value survives the pipe" \
    '2' \
    "$(printf '%s' '{"items":[{"n":"bob","age":20},{"n":"ann","age":30},{"n":"cy","age":10}]}' \
        | "$BIN/jtFilter" /items age --gt 15 | "$BIN/jtSort" /items n \
        | "$BIN/jtLen" /items /count | "$BIN/jtGet" /count)"

check_stdout "jtGet --default for a missing path" '"fallback"' \
    '{"a":1}' "$BIN/jtGet" /nope --default '"fallback"'

check_stdout "jtType reports the type at a path" '"array"' \
    '{"a":[1]}' "$BIN/jtType" /a

check_stdout "jtSet -p creates intermediate objects" '{"items":[{"sub":1,"x":1}]}' \
    '{"items":[{"sub":1}]}' "$BIN/jtSet" /items/0/x 1 -p

# --- the error contract ------------------------------------------------------
check_err_exact "typo hint is spelled exactly as README documents it" \
    '{"name":"harri"}' 'Error at /nam: path not found. did you mean /name?' \
    "$BIN/jtMove" /nam /x

check_err "a missing path is not silently created" '{"user":1}' "$BIN/jtRemove" /usr
check_err "jtGet with no value and no --default fails" '{"a":1}' "$BIN/jtGet" /nope
check_err "selecting two paths onto one key fails" '{"a":{"n":1},"b":{"n":2}}' \
    "$BIN/jtSelect" /a/n /b/n
check_err "sorting a non-array fails" '{"user":1}' "$BIN/jtSort" n
check_err "a zip with unequal lists fails" '{"k":["a","b"],"v":[1]}' "$BIN/jtZip" /k /v
check_err "filtering mixed types fails instead of coercing" '[{"a":"x"}]' \
    "$BIN/jtFilter" a --gt 1
check_err "an out-of-range array index fails" '{"a":[1,2]}' "$BIN/jtSet" /a/5 9
check_err "an empty positional argument fails" '{"a":1}' "$BIN/jtSet" "" 5

printf 'cli smoke: %s ok, %s failed\n' "$ok" "$failed"
[ "$failed" -eq 0 ] || exit 1
exit 0
