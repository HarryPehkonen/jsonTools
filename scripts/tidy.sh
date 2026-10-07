#!/usr/bin/env bash
#
# tidy — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.tidy] cmd = "scripts/tidy.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "tidy (clang-tidy, value-only checks)"
    require_tool clang-tidy tidy || return 0
    if [ ! -f "$CI_BUILD_DIR/compile_commands.json" ]; then
        # shellcheck disable=SC2046
        cmake -S . -B "$CI_BUILD_DIR" -DJT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -DCMAKE_BUILD_TYPE="$CI_BUILD_TYPE" \
            $(cmake_flag_jsom) > "$CI_LOG_DIR/tidy-configure.log" 2>&1 \
            || ci_fail tidy "cmake configure failed (tidy needs compile_commands.json)" "$CI_LOG_DIR/tidy-configure.log"
    fi
    local -a sources=()
    mapfile -t sources < <(ci_tidy_sources)

    # A compile database that exists is not a compile database that covers this repo:
    # CMake writes one per directory that enables it, and until the guard landed (kanban
    # t_772eabd9) JSOM's subdirectory enabled it for itself — so build/compile_commands.json
    # could hold JSOM's few translation units and none of ours. clang-tidy then analyses
    # nothing, cannot find a single header, and still prints "findings". This gate passes
    # -DCMAKE_EXPORT_COMPILE_COMMANDS=ON to the top level, which is what puts BOTH sets in
    # the file (a subdirectory inherits the consumer's directory scope). Prove coverage
    # before believing the result.
    local covered=0 uncovered=0 src
    for src in "${sources[@]}"; do
        if grep -qF "\"$REPO_ROOT/$src\"" "$CI_BUILD_DIR/compile_commands.json"; then
            covered=$((covered + 1))
        else
            uncovered=$((uncovered + 1))
            printf '    not in the compile database: %s\n' "$src"
        fi
    done
    if [ "$uncovered" -gt 0 ]; then
        ci_fail tidy "$uncovered of ${#sources[@]} sources are not in $CI_BUILD_DIR/compile_commands.json (found $covered) — configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON, as the build stage does, before tidy can say anything about them"
    fi
    printf '    compile database covers all %s sources\n' "$covered"
    local start elapsed
    start=$(date +%s)
    printf '%s\n' "${sources[@]}" \
        | xargs -P "$CI_JOBS" -n 1 clang-tidy -p "$CI_BUILD_DIR" > "$CI_LOG_DIR/tidy.log" 2>&1
    elapsed=$(( $(date +%s) - start ))
    local findings
    findings=$(grep -cE 'warning:|error:' "$CI_LOG_DIR/tidy.log" || true)
    if [ "${findings:-0}" -gt 0 ] && [ -f "$CI_TIDY_BASELINE" ]; then
        local new_findings
        # Both sides go through tidy_key, and both sides are de-duplicated: one key per
        # distinct finding. A baseline captured by `--write-tidy-baseline` is already in
        # that form; anything else in the file is normalised here rather than trusted.
        new_findings=$(comm -13 \
            <(tidy_key < "$CI_TIDY_BASELINE" | sort -u) \
            <(grep -E "warning:|error:" "$CI_LOG_DIR/tidy.log" | tidy_key | sort -u) | wc -l)
        if [ "$new_findings" -gt 0 ]; then
            ci_fail tidy "$new_findings new finding(s) vs $CI_TIDY_BASELINE" "$CI_LOG_DIR/tidy.log"
        fi
        printf '    no new findings vs %s (%s total, %ss)\n' "$CI_TIDY_BASELINE" "$findings" "$elapsed"
    elif [ "${findings:-0}" -gt 0 ]; then
        grep -E 'warning:|error:' "$CI_LOG_DIR/tidy.log" | sed 's/^/      /' | head -20
        ci_fail tidy "$findings finding(s) in ${#sources[@]} files, ${elapsed}s, and no baseline file — accept them in one step with 'scripts/write-tidy-baseline.sh', or fix them; see .ci.env.example" "$CI_LOG_DIR/tidy.log"
    else
        printf '    %s files clean, %ss\n' "${#sources[@]}" "$elapsed"
    fi
    ci_pass tidy
}

run_stage "$@"
