#!/usr/bin/env bash
#
# version — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.version] cmd = "scripts/version.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "version (one string, reported by every binary)"
    local header_version cmake_version
    header_version=$(sed -n 's/.*JT_VERSION\[\] *= *"\([^"]*\)".*/\1/p' include/jt/version.hpp | head -1)
    cmake_version=$(sed -n 's/^project(jsonTools VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)
    if [ -z "$header_version" ] || [ "$header_version" != "$cmake_version" ]; then
        printf '    include/jt/version.hpp says "%s", CMakeLists project VERSION says "%s"\n' \
            "${header_version:-<missing>}" "${cmake_version:-<missing>}"
        ci_fail version "the two version strings disagree (include/jt/version.hpp is the one the tools print)"
    fi
    printf '    version string: %s (include/jt/version.hpp == CMake project VERSION)\n' "$header_version"

    local binary name reported checked=0 wrong=0
    for binary in "$CI_BUILD_DIR"/jt*; do
        [ -x "$binary" ] || continue
        name=$(basename "$binary")
        case "$name" in jt_tests | lib*) continue ;; esac
        reported=$("$binary" --version 2>&1 | head -1)
        checked=$((checked + 1))
        if [ "$reported" != "$name (jsonTools) $header_version" ]; then
            printf '    %s --version printed: %s\n' "$name" "$reported"
            wrong=$((wrong + 1))
        fi
    done
    if [ "$checked" -eq 0 ]; then
        ci_fail version "no tool binaries in $CI_BUILD_DIR — run the build stage first"
    fi
    if [ "$wrong" -gt 0 ]; then
        ci_fail version "$wrong of $checked binaries do not report the version string"
    fi
    printf '    %s binaries report %s\n' "$checked" "$header_version"
    ci_pass version
}

run_stage "$@"
