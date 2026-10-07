#!/usr/bin/env bash
#
# pristine — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.pristine] cmd = "scripts/pristine.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "pristine (does the COMMITTED tree build on its own?)"
    local tmp
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/jsonTools-pristine-XXXXXX")
    RUN_TMP_DIRS+=("$tmp")
    if ! git archive HEAD | tar -x -C "$tmp"; then
        ci_fail pristine "git archive HEAD failed"
    fi
    printf '    HEAD checked out: %s files\n' "$(find "$tmp" -type f | wc -l)"

    if [ -n "$CI_JSOM_DIR" ]; then
        # An absolute path on purpose: the checkout is in a temp dir, so ../JSOM is not it.
        printf '    JSOM from %s @ %s\n' "$CI_JSOM_DIR" "$(rev_of "$CI_JSOM_DIR")"
    else
        printf '    JSOM will be fetched from GitHub — no CI_JSOM_DIR, so this run is not hermetic\n'
    fi

    cmake -S "$tmp" -B "$tmp/build" -DJT_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE="$CI_BUILD_TYPE" \
        "$(cmake_flag_jsom)" > "$CI_LOG_DIR/pristine-configure.log" 2>&1 \
        || ci_fail pristine "a fresh checkout of HEAD does not even configure (a needed file is not committed)" "$CI_LOG_DIR/pristine-configure.log"
    cmake --build "$tmp/build" -j "$CI_JOBS" > "$CI_LOG_DIR/pristine-build.log" 2>&1 \
        || ci_fail pristine "a fresh checkout of HEAD does not build" "$CI_LOG_DIR/pristine-build.log"
    "$tmp/build/jt_tests" > "$CI_LOG_DIR/pristine-tests.log" 2>&1 \
        || ci_fail pristine "tests fail on a fresh checkout of HEAD" "$CI_LOG_DIR/pristine-tests.log"
    tail -n 2 "$CI_LOG_DIR/pristine-tests.log" | sed 's/^/      /'
    if [ "$CI_KEEP_TMP" = "1" ]; then
        printf '    kept: %s\n' "$tmp"
    else
        rm -rf "$tmp"
    fi
    ci_pass pristine
}

run_stage "$@"
