#!/usr/bin/env bash
#
# jsonTools local CI — the gates from CODING_STANDARDS.md, in one script.
#
# from AI-DEV-STARTER (plunk-in kit), templates/cpp/ci.sh. This copy deviates on purpose;
# the differences are listed here so nobody has to diff it against the kit:
#   - stages added: cli, fuzz, wire (project-shaped; the kit's template omits them)
#   - stages dropped: tsan (no threads here yet), std, conform
#   - `version` IS the kit's artifact-identity check, in its three-copy C++ form:
#     include/jt/version.hpp == CMake project VERSION == what every jt* binary prints
#   - CI_JSOM_DIR defaults to the sibling ../JSOM checkout, so the gate stays offline
#   - CI_TEST_CMD is unused: there is one gtest binary, ./$CI_BUILD_DIR/jt_tests
#
# No GitHub, no network, no framework: this is what the git hooks in .githooks/ run,
# and you can run it by hand at any time (it is non-destructive — nothing is
# committed, staged, reverted or reformatted for you).
#
#   tools/ci.sh                      # all stages
#   tools/ci.sh build tests          # just these stages, in the order given
#   tools/ci.sh --list               # what the stages are
#   tools/ci.sh --help
#
#   git config core.hooksPath .githooks     # one-time, per clone, enables the hooks
#
# Configuration lives in .ci.env (gitignored, optional); every knob has a default
# here, so the repo works with no config at all. See .ci.env.example.
#
# Exit status: 0 only if every stage that ran passed. A failing stage stops the run,
# prints why, and leaves its full output in .ci-logs/<stage>.log. The last line of a run
# is always GATE PASSED or GATE FAILED, and a failure names every requested stage that
# never ran (BLOCK <stage>), so a stage that did not run is never read as one that passed.

set -uo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT" || exit 1

# No colour from the tools: this script greps their output (warning:, error:, DONE, tests
# from) and ANSI escapes defeat the greps. The escapes this script prints itself are for
# the human reading the terminal.
export NO_COLOR=1

# ---------------------------------------------------------------- defaults + config
CI_JOBS=${CI_JOBS:-$(nproc 2>/dev/null || echo 4)}
CI_BUILD_DIR=${CI_BUILD_DIR:-build}
CI_ASAN_BUILD_DIR=${CI_ASAN_BUILD_DIR:-build-asan}
CI_FUZZ_BUILD_DIR=${CI_FUZZ_BUILD_DIR:-build-fuzz}
CI_FUZZ_SECONDS=${CI_FUZZ_SECONDS:-60}         # libFuzzer smoke budget; the nightly cron owns the long campaign
CI_LOG_DIR=${CI_LOG_DIR:-.ci-logs}
CI_STRICT_TOOLS=${CI_STRICT_TOOLS:-0}          # 1 = a missing tool fails instead of SKIPping
CI_KEEP_TMP=${CI_KEEP_TMP:-0}                  # 1 = keep the pristine-checkout temp dir
CI_DEFAULT_STAGES=${CI_DEFAULT_STAGES:-"tree format build tests cli asan fuzz tidy wire version pristine"}
CI_TIDY_BASELINE=${CI_TIDY_BASELINE:-.ci/tidy-baseline.txt}
CI_JSOM_DIR=${CI_JSOM_DIR:-}                   # empty = let CMake FetchContent JSOM

if [ -f .ci.env ]; then
    # shellcheck disable=SC1091
    . ./.ci.env
fi

# Default to the sibling JSOM checkout when it is there: building the dependency from
# a checkout we already have keeps the gate offline and attributable, where FetchContent
# needs the network and floats on JSOM's main branch. Every run prints which revision it
# used, so a break that comes from JSOM is visible instead of mysterious.
if [ -z "$CI_JSOM_DIR" ] && [ -f "$REPO_ROOT/../JSOM/CMakeLists.txt" ]; then
    CI_JSOM_DIR=$(cd "$REPO_ROOT/../JSOM" && pwd)
fi

REQUIRE_CLEAN=0
ALLOW_UNTRACKED=0
STAGES_REQUESTED=()

# ---------------------------------------------------------------- plumbing
RESULT_LINES=()
FAILED_STAGE=""
RAN_STAGES=()
RUN_TMP_DIRS=()

usage() {
    sed -n '2,31p' "$0" | sed 's/^# \{0,1\}//'
    cat <<'EOF'

Stages:
  tree        every file is committed or ignored (untracked+unignored fails), plus a
              .gitignore audit; --require-clean also fails on uncommitted changes
  format      clang-format drift in the files this branch touches — dry run against the
              repo .clang-format
  build       cmake configure + build (tests on, compile database on), zero warnings
  tests       ./<build>/jt_tests
  cli         the jt* binaries end to end: the README examples, and the error contract
  asan        build-asan (ASan+UBSan) + the same test suite under the sanitizers
  fuzz        the libFuzzer harness over the pointer/argv surface, for
              CI_FUZZ_SECONDS (default 60) — long campaigns are the nightly cron's
  tidy        clang-tidy over src/, tests/ and fuzz/ (repo .clang-tidy); zero findings
  wire        every tool is built, installed, depended on by the tests, exercised and
              documented — the guard for the next tool you add
  version     one version string: include/jt/version.hpp == CMake project VERSION ==
              what every binary prints for --version
  pristine    git archive HEAD -> temp dir -> configure, build, test: proves the
              COMMITTED tree is complete (catches files that are uncommitted or ignored)

Options:
  --require-clean     make the tree stage fail when tracked files have uncommitted edits
  --allow-untracked   do not fail when untracked, unignored files exist (deliberate escape)
  --strict-tools      a missing tool (clang-format/clang-tidy) fails instead of skipping
  --list              list the stages and exit
  --help              this text
EOF
}

log() { printf '%s\n' "$*"; }

ci_begin() {
    printf '\n\033[1m==> %s\033[0m\n' "$1"
}

ci_pass() {
    RESULT_LINES+=("pass  $1")
}

ci_skip() {
    RESULT_LINES+=("SKIP  $1 ($2)")
    printf '    SKIP: %s\n' "$2"
}

ci_fail() {
    local name="$1" reason="$2" logfile="${3:-}"
    RESULT_LINES+=("FAIL  $name")
    printf '\n\033[1;31mFAILED: %s — %s\033[0m\n' "$name" "$reason"
    if [ -n "$logfile" ] && [ -f "$logfile" ]; then
        printf '    last output from %s:\n' "$logfile"
        tail -n 25 "$logfile" | sed 's/^/      /'
        printf '    full log: %s\n' "$logfile"
    fi
    FAILED_STAGE="$name"
    summary
    printf '\nGATE FAILED\n' >&2
    exit 1
}

summary() {
    printf '\n--- ci summary ---\n'
    printf '  jsonTools @ %s (%s) | JSOM @ %s%s\n' \
        "$(rev_of "$REPO_ROOT")" "$(git rev-parse --abbrev-ref HEAD 2>/dev/null)" \
        "$(rev_of "$CI_JSOM_DIR")" "${CI_JSOM_DIR:+ ($CI_JSOM_DIR)}"
    local line
    for line in "${RESULT_LINES[@]}"; do printf '  %s\n' "$line"; done
    if [ -n "$FAILED_STAGE" ]; then
        printf '  stopped at: %s\n' "$FAILED_STAGE"
        # A stage that never ran because an earlier one failed must not read as a stage
        # that passed. Once the build fails, the stages behind it have nothing trustworthy
        # to say, so they are named as blocked rather than silently omitted.
        local s r ran
        for s in "${STAGES_REQUESTED[@]:-}"; do
            [ -n "$s" ] || continue
            ran=0
            for r in "${RAN_STAGES[@]:-}"; do
                [ "$r" = "$s" ] && ran=1
            done
            if [ "$ran" = "0" ] && [ "$s" != "$FAILED_STAGE" ]; then
                printf '  BLOCK %s (did not run: the run stopped at %s)\n' "$s" "$FAILED_STAGE"
            fi
        done
    fi
}

rev_of() {  # a revision you can recognise, or an honest "(none)" / "(dir +dirty)"
    local dir="${1:-}" sha
    if [ -z "$dir" ]; then
        printf '(none)'
        return 0
    fi
    if ! sha=$(git -C "$dir" rev-parse --short HEAD 2>/dev/null); then
        printf '(not a git checkout)'
        return 0
    fi
    if [ -n "$(git -C "$dir" status --porcelain 2>/dev/null)" ]; then
        printf '%s+dirty' "$sha"
    else
        printf '%s' "$sha"
    fi
}

require_tool() {
    local tool="$1" stage="$2"
    if command -v "$tool" >/dev/null 2>&1; then
        return 0
    fi
    if [ "$CI_STRICT_TOOLS" = "1" ]; then
        ci_fail "$stage" "$tool not installed (CI_STRICT_TOOLS=1)"
    fi
    ci_skip "$stage" "$tool not installed — gate not exercised on this machine"
    return 1
}

# clang-tidy needs an entry in compile_commands.json per translation unit, so headers are
# not passed here: diagnostics inside include/jt/ still surface through the sources that
# include them (that is what .clang-tidy's HeaderFilterRegex is for).
# fuzz/*.cpp is compile-database covered by CMakeLists.txt's jt_fuzz_harness object
# library — the harness is clang-only as an EXECUTABLE, but the default build still
# compiles it, which is what puts it in build/compile_commands.json.
ci_tidy_sources() {
    git ls-files --cached --others --exclude-standard -- 'src/*.cpp' 'tests/*.cpp' 'fuzz/*.cpp' | sort -u
}

cmake_flag_jsom() {
    # An empty value is deliberate and correct: CMakeLists.txt falls back to FetchContent
    # when JSOM_SOURCE_DIR is empty, so one code path covers both cases.
    printf -- '-DJSOM_SOURCE_DIR=%s' "$CI_JSOM_DIR"
}

mkdir -p "$CI_LOG_DIR"

# ---------------------------------------------------------------- stages
stage_tree() {
    ci_begin "tree (every file committed or ignored)"
    # The gate creates these; a .gitignore that does not cover them would make the next
    # run fail the moment it writes a log. Create them first: git check-ignore cannot
    # match a directory pattern (build-*/) against a path that does not exist yet.
    mkdir -p "$CI_BUILD_DIR" "$CI_ASAN_BUILD_DIR" "$CI_FUZZ_BUILD_DIR" "$CI_LOG_DIR"

    local untracked
    untracked=$(git ls-files --others --exclude-standard)
    if [ -n "$untracked" ]; then
        printf '    not committed and not ignored:\n'
        printf '%s\n' "$untracked" | sed 's/^/      /'
        if [ "$ALLOW_UNTRACKED" = "1" ]; then
            printf '    (not failing: --allow-untracked was passed)\n'
        else
            ci_fail tree "$(printf '%s\n' "$untracked" | wc -l) file(s) are neither committed nor ignored — git add them, or add a .gitignore rule"
        fi
    else
        printf '    no untracked, unignored files\n'
    fi

    local dirty
    dirty=$(git status --porcelain --untracked-files=no)
    if [ -n "$dirty" ]; then
        printf '    uncommitted changes to tracked files:\n'
        printf '%s\n' "$dirty" | sed 's/^/      /'
        if [ "$REQUIRE_CLEAN" = "1" ]; then
            ci_fail tree "uncommitted changes to tracked files (that is the point of the push gate)"
        fi
        printf '    (not failing: --require-clean was not passed)\n'
    else
        printf '    no uncommitted changes to tracked files\n'
    fi

    local tracked_ignored
    tracked_ignored=$(git ls-files -i -c --exclude-standard)
    if [ -n "$tracked_ignored" ]; then
        printf '%s\n' "$tracked_ignored" > "$CI_LOG_DIR/tree.log"
        ci_fail tree "tracked files matched by .gitignore (stale rules) — see $CI_LOG_DIR/tree.log"
    fi

    local path missing=0
    for path in "$CI_BUILD_DIR" "$CI_ASAN_BUILD_DIR" "$CI_FUZZ_BUILD_DIR" "$CI_LOG_DIR" ".ci.env"; do
        if ! git check-ignore -q "$path" 2>/dev/null; then
            printf '    NOT ignored: %s\n' "$path"
            missing=$((missing + 1))
        fi
    done
    if [ "$missing" -gt 0 ]; then
        ci_fail tree "$missing path(s) that the gate itself creates are not in .gitignore"
    fi
    printf '    gate footprint (build dirs, %s, .ci.env) is ignored\n' "$CI_LOG_DIR"
    ci_pass tree
}

# The files the branch touched, from three sources — the branch diff, the uncommitted
# diff, and the untracked set (a brand-new file is invisible to `git diff` until it is
# staged). A checkout level with origin/main has no diff at all, so the caller falls back
# to the last commit and says so.
touched_files() {
    local base
    base="$(git merge-base HEAD origin/main 2>/dev/null || git rev-parse HEAD)"
    {
        git diff --name-only --diff-filter=ACMR "$base" HEAD
        git diff --name-only --diff-filter=ACMR HEAD
        git ls-files --others --exclude-standard
    } | sort -u
}

stage_format() {
    ci_begin "format (clang-format --dry-run — the files this branch touches)"
    require_tool clang-format format || return 0
    # Only the branch's own files, never the whole tree: every real repo carries
    # pre-existing drift nobody edited, and a check that fails on other people's files is
    # muted within a week. When you want the whole tree anyway (after a formatter bump):
    #   clang-format --dry-run -Werror $(git ls-files 'include/jt/*.hpp' 'src/*.cpp' 'tests/*.cpp' 'fuzz/*.cpp')
    local touched
    touched="$(touched_files)"
    if [ -z "$touched" ]; then
        touched="$(git show --name-only --pretty=format: HEAD | sed '/^$/d')"
        printf '    (level with origin/main: checking the last commit instead)\n'
    fi
    local -a sources=()
    local f
    while IFS= read -r f; do
        [ -n "$f" ] && [ -f "$f" ] && sources+=("$f")
    done < <(printf '%s\n' "$touched" | grep -E '\.(cpp|hpp)$')
    if [ "${#sources[@]}" -eq 0 ]; then
        printf '    nothing to check: this branch touches no C++ file\n'
        ci_pass format
        return 0
    fi
    if clang-format --dry-run -Werror "${sources[@]}" > "$CI_LOG_DIR/format.log" 2>&1; then
        printf '    %s touched file(s) conform to .clang-format\n' "${#sources[@]}"
        ci_pass format
        return 0
    fi
    grep -oE '^[^:]+\.(cpp|hpp)' "$CI_LOG_DIR/format.log" | sort -u | sed 's/^/      /'
    ci_fail format "clang-format drift in the files listed above (fix: clang-format -i <those files>)" "$CI_LOG_DIR/format.log"
}

stage_build() {
    ci_begin "build (-Werror, zero warnings)"
    # shellcheck disable=SC2046
    cmake -S . -B "$CI_BUILD_DIR" -DJT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
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

stage_tests() {
    ci_begin "tests"
    if [ ! -x "$CI_BUILD_DIR/jt_tests" ]; then
        ci_fail tests "no $CI_BUILD_DIR/jt_tests — run the build stage first (JT_BUILD_TESTS=ON)"
    fi
    "$CI_BUILD_DIR/jt_tests" > "$CI_LOG_DIR/tests.log" 2>&1 || ci_fail tests "test failures" "$CI_LOG_DIR/tests.log"
    grep -E '^\[==========\] [0-9]+ tests? from' "$CI_LOG_DIR/tests.log" | tail -1 | sed 's/^/      /'
    tail -n 1 "$CI_LOG_DIR/tests.log" | sed 's/^/      /'
    ci_pass tests
}

stage_cli() {
    ci_begin "cli (the jt* binaries end to end)"
    if ! bash "$REPO_ROOT/tools/cli_smoke.sh" "$CI_BUILD_DIR" > "$CI_LOG_DIR/cli.log" 2>&1; then
        ci_fail cli "a CLI check failed" "$CI_LOG_DIR/cli.log"
    fi
    tail -n 1 "$CI_LOG_DIR/cli.log" | sed 's/^/      /'
    ci_pass cli
}

stage_asan() {
    ci_begin "asan (ASan+UBSan build of the same suite)"
    # shellcheck disable=SC2046
    cmake -S . -B "$CI_ASAN_BUILD_DIR" -DJT_BUILD_TESTS=ON \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
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

stage_fuzz() {
    ci_begin "fuzz (libFuzzer smoke, ${CI_FUZZ_SECONDS}s)"
    require_tool clang++ fuzz || return 0
    # The harness is clang-only (libFuzzer IS clang's runtime) and gets its own build
    # directory: JT_BUILD_FUZZING=ON instruments every target there — coverage plus
    # ASan/UBSan — while only the harness links libFuzzer's main(), so the jt* tools in
    # that directory still build and link normally.
    # shellcheck disable=SC2046
    cmake -S . -B "$CI_FUZZ_BUILD_DIR" -DCMAKE_CXX_COMPILER=clang++ -DJT_BUILD_FUZZING=ON \
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

stage_tidy() {
    ci_begin "tidy (clang-tidy, value-only checks)"
    require_tool clang-tidy tidy || return 0
    if [ ! -f "$CI_BUILD_DIR/compile_commands.json" ]; then
        # shellcheck disable=SC2046
        cmake -S . -B "$CI_BUILD_DIR" -DJT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            $(cmake_flag_jsom) > "$CI_LOG_DIR/tidy-configure.log" 2>&1 \
            || ci_fail tidy "cmake configure failed (tidy needs compile_commands.json)" "$CI_LOG_DIR/tidy-configure.log"
    fi
    local -a sources
    mapfile -t sources < <(ci_tidy_sources)

    # A compile database that exists is not a compile database that covers this repo:
    # CMake writes one per directory that enables it, and the JSOM subdirectory enables
    # it for itself — so build/compile_commands.json can hold JSOM's few translation
    # units and none of ours. clang-tidy then analyses nothing, cannot find a single
    # header, and still prints "findings". Prove coverage before believing the result.
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
        new_findings=$(comm -13 \
            <(sort "$CI_TIDY_BASELINE") \
            <(grep -E "warning:|error:" "$CI_LOG_DIR/tidy.log" | sed 's/:[0-9]*:[0-9]*:/:/' | sort) | wc -l)
        if [ "$new_findings" -gt 0 ]; then
            ci_fail tidy "$new_findings new finding(s) vs $CI_TIDY_BASELINE" "$CI_LOG_DIR/tidy.log"
        fi
        printf '    no new findings vs %s (%s total, %ss)\n' "$CI_TIDY_BASELINE" "$findings" "$elapsed"
    elif [ "${findings:-0}" -gt 0 ]; then
        grep -E 'warning:|error:' "$CI_LOG_DIR/tidy.log" | sed 's/^/      /' | head -20
        ci_fail tidy "$findings finding(s) in ${#sources[@]} files, ${elapsed}s (no baseline file)" "$CI_LOG_DIR/tidy.log"
    else
        printf '    %s files clean, %ss\n' "${#sources[@]}" "$elapsed"
    fi
    ci_pass tidy
}

stage_wire() {
    ci_begin "wire (every tool built, installed, tested, documented)"
    if ! command -v python3 >/dev/null 2>&1; then
        ci_skip wire "python3 not installed"
        return 0
    fi
    if ! python3 "$REPO_ROOT/tools/check_wiring.py" > "$CI_LOG_DIR/wire.log" 2>&1; then
        ci_fail wire "a tool is not fully wired (the missing wires are listed above)" "$CI_LOG_DIR/wire.log"
    fi
    grep -E '^wire:' "$CI_LOG_DIR/wire.log" | sed 's/^/      /'
    ci_pass wire
}

stage_version() {
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

stage_pristine() {
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

    cmake -S "$tmp" -B "$tmp/build" -DJT_BUILD_TESTS=ON "$(cmake_flag_jsom)" \
        > "$CI_LOG_DIR/pristine-configure.log" 2>&1 \
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

cleanup() {
    local dir
    if [ "$CI_KEEP_TMP" != "1" ]; then
        for dir in "${RUN_TMP_DIRS[@]:-}"; do
            [ -n "$dir" ] && [ -d "$dir" ] && rm -rf "$dir"
        done
    fi
}
trap cleanup EXIT

# ---------------------------------------------------------------- dispatch
while [ $# -gt 0 ]; do
    case "$1" in
    --help | -h)
        usage
        exit 0
        ;;
    --list)
        printf 'default stages: %s\n' "$CI_DEFAULT_STAGES"
        # Derived from the defined functions, so this cannot drift from the stages the
        # script actually implements.
        printf 'stages:'
        for fn in $(declare -F | awk '{print $3}' | grep '^stage_' | sed 's/^stage_//' | sort); do
            printf ' %s' "$fn"
        done
        printf '\n'
        exit 0
        ;;
    --require-clean) REQUIRE_CLEAN=1 ;;
    --allow-untracked) ALLOW_UNTRACKED=1 ;;
    --strict-tools) CI_STRICT_TOOLS=1 ;;
    -*)
        printf 'unknown option: %s (try --help)\n' "$1" >&2
        exit 2
        ;;
    *) STAGES_REQUESTED+=("$1") ;;
    esac
    shift
done

if [ ${#STAGES_REQUESTED[@]} -eq 0 ]; then
    # shellcheck disable=SC2206
    STAGES_REQUESTED=($CI_DEFAULT_STAGES)
fi

printf '\033[1mjsonTools local CI\033[0m — %s stage(s): %s\n' "${#STAGES_REQUESTED[@]}" "${STAGES_REQUESTED[*]}"
printf '  jsonTools @ %s (%s) | JSOM @ %s%s\n' \
    "$(rev_of "$REPO_ROOT")" "$(git rev-parse --abbrev-ref HEAD 2>/dev/null)" \
    "$(rev_of "$CI_JSOM_DIR")" "${CI_JSOM_DIR:+ ($CI_JSOM_DIR)}"

START=$(date +%s)
for stage in "${STAGES_REQUESTED[@]}"; do
    if ! declare -F "stage_$stage" > /dev/null; then
        printf 'unknown stage: %s (try --list)\n' "$stage" >&2
        exit 2
    fi
    "stage_$stage"
    RAN_STAGES+=("$stage")
done
ELAPSED=$(( $(date +%s) - START ))
summary
printf '\nall %s stage(s) passed in %ss\nGATE PASSED\n' "${#STAGES_REQUESTED[@]}" "$ELAPSED"
