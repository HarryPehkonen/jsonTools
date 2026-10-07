#!/usr/bin/env bash
#
# build — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.build] cmd = "scripts/build.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "build (-Werror, zero warnings)"
    # shellcheck disable=SC2046
    cmake -S . -B "$CI_BUILD_DIR" -DJT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_BUILD_TYPE="$CI_BUILD_TYPE" \
        $(cmake_flag_jsom) > "$CI_LOG_DIR/configure.log" 2>&1 \
        || ci_fail build "cmake configure failed" "$CI_LOG_DIR/configure.log"
    cmake --build "$CI_BUILD_DIR" -j "$CI_JOBS" > "$CI_LOG_DIR/build.log" 2>&1 \
        || ci_fail build "build failed (-Werror is on: a warning is a build failure)" "$CI_LOG_DIR/build.log"
    # -Werror only covers the targets it is wired onto, so a new tool that never made it
    # into the CMake tool list would build with warnings and pass. Count them here too.
    local warns
    warns=$(grep -c 'warning:' "$CI_LOG_DIR/build.log" || true)
    if [ "${warns:-0}" -gt 0 ]; then
        grep 'warning:' "$CI_LOG_DIR/build.log" | head -5 | sed 's/^/      /'
        ci_fail build "$warns compiler warning(s) in a target that -Werror does not cover (see the wire stage)" "$CI_LOG_DIR/build.log"
    fi
    printf '    built %s tool binaries + jt_core with no warnings\n' "$(ls -1 "$CI_BUILD_DIR"/jt* 2>/dev/null | grep -vc jt_tests)"
    ci_pass build
}

run_stage "$@"
