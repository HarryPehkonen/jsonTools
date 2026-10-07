#!/usr/bin/env bash
#
# asan — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.asan] cmd = "scripts/asan.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "asan (ASan+UBSan build of the same suite)"
    # shellcheck disable=SC2046
    cmake -S . -B "$CI_ASAN_BUILD_DIR" -DJT_BUILD_TESTS=ON \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
        -DCMAKE_BUILD_TYPE="$CI_BUILD_TYPE" \
        $(cmake_flag_jsom) > "$CI_LOG_DIR/asan-configure.log" 2>&1 \
        || ci_fail asan "cmake configure failed" "$CI_LOG_DIR/asan-configure.log"
    cmake --build "$CI_ASAN_BUILD_DIR" -j "$CI_JOBS" > "$CI_LOG_DIR/asan-build.log" 2>&1 \
        || ci_fail asan "sanitizer build failed" "$CI_LOG_DIR/asan-build.log"
    if ! UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ASAN_OPTIONS=detect_leaks=1 \
        "$CI_ASAN_BUILD_DIR/jt_tests" > "$CI_LOG_DIR/asan-tests.log" 2>&1; then
        ci_fail asan "ASan/UBSan reported something" "$CI_LOG_DIR/asan-tests.log"
    fi
    tail -n 1 "$CI_LOG_DIR/asan-tests.log" | sed 's/^/      /'
    ci_pass asan
}

run_stage "$@"
