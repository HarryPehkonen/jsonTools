#!/usr/bin/env bash
#
# fuzz — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.fuzz] cmd = "scripts/fuzz.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "fuzz (libFuzzer smoke, ${CI_FUZZ_SECONDS}s)"
    require_tool clang++ fuzz || return 0
    # The harness is clang-only (libFuzzer IS clang's runtime) and gets its own build
    # directory: JT_BUILD_FUZZING=ON instruments every target there — coverage plus
    # ASan/UBSan — while only the harness links libFuzzer's main(), so the jt* tools in
    # that directory still build and link normally.
    # shellcheck disable=SC2046
    cmake -S . -B "$CI_FUZZ_BUILD_DIR" -DCMAKE_CXX_COMPILER=clang++ -DJT_BUILD_FUZZING=ON \
        -DCMAKE_BUILD_TYPE="$CI_BUILD_TYPE" \
        $(cmake_flag_jsom) > "$CI_LOG_DIR/fuzz-configure.log" 2>&1 \
        || ci_fail fuzz "cmake configure failed (the fuzz target needs clang++)" "$CI_LOG_DIR/fuzz-configure.log"
    cmake --build "$CI_FUZZ_BUILD_DIR" --target build_fuzzer -j "$CI_JOBS" \
        > "$CI_LOG_DIR/fuzz-build.log" 2>&1 \
        || ci_fail fuzz "the fuzz target did not build" "$CI_LOG_DIR/fuzz-build.log"

    # Two libFuzzer facts, both verified by hitting them: it refuses to start when the
    # corpus directory does not exist (a fresh clone has none), and without
    # -artifact_prefix the crash artifact is written to the current directory, where the
    # failure report will never look for it. The corpus persists between runs, so it
    # accumulates coverage over this checkout's life.
    local corpus="$CI_FUZZ_BUILD_DIR/corpus"
    mkdir -p "$corpus"
    if ! UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ASAN_OPTIONS=detect_leaks=1 \
        "$CI_FUZZ_BUILD_DIR/fuzz_jt" "$corpus" "$REPO_ROOT/fuzz/seeds" \
        -dict="$REPO_ROOT/fuzz/jt.dict" -artifact_prefix="$corpus/" \
        -max_total_time="$CI_FUZZ_SECONDS" -print_final_stats=1 \
        > "$CI_LOG_DIR/fuzz.log" 2>&1; then
        grep -m1 'Test unit written to' "$CI_LOG_DIR/fuzz.log" | sed 's/^/      reproducer: /'
        printf '      replay it with: %s/fuzz_jt <artifact>\n' "$CI_FUZZ_BUILD_DIR"
        ci_fail fuzz "libFuzzer reported a finding (crash, leak or UB) within ${CI_FUZZ_SECONDS}s" "$CI_LOG_DIR/fuzz.log"
    fi
    # Belt and braces: a sanitizer report that never reaches the exit status would make
    # this stage green on a real finding — measured 2026-09-19, a forked child's ASan
    # error exits 1, exactly like an expected rejection. The harness runs everything
    # in-process, so any report in this log is a finding.
    if grep -qE 'ERROR: (libFuzzer|AddressSanitizer|UndefinedBehaviorSanitizer)|SUMMARY: (AddressSanitizer|UndefinedBehaviorSanitizer)' \
        "$CI_LOG_DIR/fuzz.log"; then
        ci_fail fuzz "the fuzz log carries a sanitizer/libFuzzer report even though the run exited 0" "$CI_LOG_DIR/fuzz.log"
    fi
    # The last DONE line carries coverage/features/corpus size and the input rate; the
    # "Done N runs" line carries the input count. Together they say whether the smoke
    # actually explored anything, which a bare exit status cannot.
    grep -E 'DONE|^Done [0-9]+ runs' "$CI_LOG_DIR/fuzz.log" | tail -2 | sed 's/^/      /'
    ci_pass fuzz
}

run_stage "$@"
