#!/usr/bin/env bash
#
# jsonTools's gate: the configuration every stage reads, and the helpers they share.
#
# The POLICY is gate.toml (which stages exist, which tier runs which of them, how a failure
# is recognised). The ENGINE is kit-ci, one binary installed once per machine
# (cmake --install build --prefix ~/.local). Everything a stage needs BEYOND that policy -
# a build dir, a job count, a fuzz budget - lives HERE, so the policy file stays a list of
# stages.
#
# SOURCED, never executed: scripts/gate.sh does not need it; every stage script does
# (`. "$(dirname "$0")/gate-env.sh"`). The knobs are the .ci.env knobs the old tools/ci.sh
# carried, each with the default it had there, and .ci.env is still sourced last, so a
# machine with no .ci.env behaves exactly as it did before the conversion
# (2026-10-06, card t_075c0a6f).

set -uo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT" || exit 1

# No colour from the tools: this script greps their output (warning:, error:, DONE, tests
# from) and ANSI escapes defeat the greps. The escapes this script prints itself are for
# the human reading the terminal.
export NO_COLOR=1

# git exports GIT_INDEX_FILE to a hook when the commit is made with a PATHSPEC
# (`git commit -- <path>`): it names git's TEMPORARY index for that one commit, not this
# repository's index, and every process a hook starts inherits it. Most of this gate's own
# git calls read THIS repo and survive it; three do not, and they are the whole exposure:
# rev_of() asks the sibling JSOM checkout for its revision and its dirtiness with
# `git -C "$dir" rev-parse` and `git -C "$dir" status --porcelain` (that is the `JSOM @ …`
# half of this gate's header), the FetchContent fallback below has cmake's update step run
# inside a fresh clone, and this repo's own kitprobes scripts run git in throwaway
# repositories by design. Handed THIS repo's index, the first two read entries whose blobs
# that other repository's object store does not have, and the first one it lacks is fatal,
#     fatal: unable to read d731f7cb4c9d558cde8e134a2f7ae80ee93bfeaf      # measured, rc 128
# (jsonTools against the sibling JSOM, 2026-09-20). In rev_of() it is silent rather than
# loud — the error is suppressed, so the header quietly loses its `+dirty` marker, the
# signal that says the dependency checkout a build used is not the one that was tested —
# while on Computo the same shape made a pathspec commit fail its OWN gate at `build` with a
# dependency-update error, on a tree that builds fine (`CMake Error at
# .../jsom-populate-gitupdate.cmake:186 (message): Failed to get the status`, cards
# t_9541aa62 -> t_0a9a0018).
#
# Unset it once, here, rather than `env -u` on the configure lines: the variable reaches
# everything the gate starts — every configure, the `pristine` archive build, the kitprobes
# scripts — so a per-invocation fix would cover the instance and leave the class. Measured
# before choosing the place (a real pathspec commit in a throwaway clone, with the gate's
# own stages run both ways): every input the `tree`/`format` stages read is identical and
# their output is byte-identical. Ported from the kit's templates/cpp/ci.sh at f9c3300;
# tools/kit-probes/git-index-file.sh holds this copy to it.
unset GIT_INDEX_FILE

# ---------------------------------------------------------------- defaults + config
CI_JOBS=${CI_JOBS:-$(nproc 2>/dev/null || echo 4)}
CI_BUILD_DIR=${CI_BUILD_DIR:-build}
CI_ASAN_BUILD_DIR=${CI_ASAN_BUILD_DIR:-build-asan}
CI_FUZZ_BUILD_DIR=${CI_FUZZ_BUILD_DIR:-build-fuzz}
CI_FUZZ_SECONDS=${CI_FUZZ_SECONDS:-60}         # libFuzzer smoke budget; the nightly cron owns the long campaign
CI_LOG_DIR=${CI_LOG_DIR:-.ci-logs}
CI_STRICT_TOOLS=${CI_STRICT_TOOLS:-0}          # 1 = a missing tool fails instead of SKIPping
CI_KEEP_TMP=${CI_KEEP_TMP:-0}                  # 1 = keep the pristine-checkout temp dir
# The two hook tiers, ONE definition each. Both hooks name a tier instead of repeating a list, so a
# stage added below cannot be run by a hand run and skipped by a push (or the reverse) — in FSMTable
# the gate had gained `fuzz` while the installed pre-push still named the kit's original eleven, so
# every push skipped the fuzzer. The lines below are what probes/hook-tiers-agree.sh compares the two
# variables against, and the header above says the same thing in its own words.
#
#   fast  (pre-commit)  tree format build tests
#   full  (pre-push)    --require-clean tree format kitprobes build tests cli asan fuzz tidy wire version pristine
#
# `format` is in the fast tier deliberately: it is the one check that says "the file you are about to
# commit is not the file clang-format would write", it costs well under a second on a warm tree, and
# its absence from a fast tier is exactly how an unformatted commit reached FSMTable's main branch on
# 2026-10-06. `full` IS the default list, so a hand run and a push run the same eleven stages and only
# --require-clean differs.
CI_FAST_STAGES=${CI_FAST_STAGES:-"tree format build tests"}
CI_FULL_STAGES=${CI_FULL_STAGES:-"tree format kitprobes build tests cli asan fuzz tidy wire version pristine"}
CI_DEFAULT_STAGES=${CI_DEFAULT_STAGES:-$CI_FULL_STAGES}
CI_TIDY_BASELINE=${CI_TIDY_BASELINE:-.ci/tidy-baseline.txt}
CI_JSOM_DIR=${CI_JSOM_DIR:-}                   # empty = let CMake FetchContent JSOM

# Build type for every configure call in this gate (build, asan, fuzz, tidy, pristine).
# Release is deliberate — the fuzz and asan stages want the optimized code path, and
# CMakeLists.txt adds -g to the code under test — and it is stated EXPLICITLY because it
# used to be inherited: JSOM's CMakeLists wrote CMAKE_BUILD_TYPE=Release into the cache with
# FORCE while entering as a subdirectory, so this repo was Release by accident of its
# dependency. JSOM no longer does that (kanban t_772eabd9), so without this line the tree
# would silently drop to CMake's -O0.
CI_BUILD_TYPE=${CI_BUILD_TYPE:-Release}

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


# The old tools/ci.sh took these as FLAGS (--require-clean, --changed, --extra-checks,
# --write-tidy-baseline, --write-coverage-baseline). kit-ci's vocabulary has no per-run flags
# for a stage: a caller sets the knob in the ENVIRONMENT it launches the gate with, and the
# default below is what the script had. .githooks/pre-push does exactly that with
# CI_REQUIRE_CLEAN=1. (Until this line the assignment overwrote whatever the caller exported,
# so the push hook's flag-as-env-var had no effect at all - measured 2026-10-06.)

REQUIRE_CLEAN=${CI_REQUIRE_CLEAN:-0}
ALLOW_UNTRACKED=0
WRITE_TIDY_BASELINE=0
STAGES_REQUESTED=()

# ---------------------------------------------------------------- plumbing
RESULT_LINES=()
FAILED_STAGE=""
RAN_STAGES=()
RUN_TMP_DIRS=()


log() { printf '%s\n' "$*"; }






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

# The comparable form of a clang-tidy finding — applied to BOTH sides of the baseline
# comparison, so the file a repo captures and the log this gate just wrote are the same
# shape:
#
#   <repo>/src/foo.cpp:42:7: warning: ...   ->   src/foo.cpp: warning: ...
#
#   * the repo root is stripped: clang-tidy reports the path it was handed by the compile
#     database, which CMake writes as an absolute path, so a baseline captured in one clone
#     names no finding in a checkout at another path (the nightly clean checkout, a
#     colleague's machine) and every inherited finding reads as new;
#   * :line:column is stripped, so the same finding after an unrelated edit above it is
#     still the same finding. This is line-blind on purpose, and the flip side is worth
#     knowing: a SECOND identical finding in a file that already has one collapses into the
#     first. Fix the baselined finding instead of growing the baseline.
#
# `tools/ci.sh --write-tidy-baseline` captures the baseline through this same function, so
# the documented way to accept findings cannot drift from the way they are compared.
tidy_key() {
    awk -v root="$REPO_ROOT/" '
        { i = index($0, root); if (i) $0 = substr($0, i + length(root)); print }' \
        | sed 's/:[0-9]*:[0-9]*:/:/'
}

cmake_flag_jsom() {
    # An empty value is deliberate and correct: CMakeLists.txt falls back to FetchContent
    # when JSOM_SOURCE_DIR is empty, so one code path covers both cases.
    printf -- '-DJSOM_SOURCE_DIR=%s' "$CI_JSOM_DIR"
}

mkdir -p "$CI_LOG_DIR"


# ---------------------------------------------------------------- stage helpers
touched_files() {
    local base
    base="$(git merge-base HEAD origin/main 2>/dev/null || git rev-parse HEAD)"
    {
        git diff --name-only --diff-filter=ACMR "$base" HEAD
        git diff --name-only --diff-filter=ACMR HEAD
        git diff --cached --name-only --diff-filter=ACMR   # a staged-only change is invisible to the line above
        git ls-files --others --exclude-standard
    } | sort -u
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

# ---------------------------------------------------------------- verdict shims
# kit-ci calls a stage and reads its EXIT STATUS: 0 is a pass, non-zero is a failure, and
# the engine names the stage and prints the first lines of a failed stage's output itself
# (SPEC.md §4). The stage bodies in this directory were split verbatim out of the old
# tools/ci.sh and still speak that script's vocabulary, so it is defined here, once:
#
#   ci_pass <stage>                 END the stage, exit 0
#   ci_fail <stage> <reason> [log]  print why, show the log's tail, exit 1
#   ci_skip <stage> <reason>        say so; the caller then exits 0 (a SKIP is not a failure,
#                                   which is also what kit-ci's `when = "tool:<name>"` means)
#   ci_begin <title>                the old banner line
ci_begin() { printf '\n==> %s\n' "$1"; }
ci_pass() { exit 0; }
ci_skip() { printf '    SKIP: %s\n' "$2"; }
ci_fail() {
    printf 'FAILED: %s — %s\n' "$1" "$2" >&2
    [ -n "${3:-}" ] && show_log "$3"
    exit 1
}
show_log() {  # show_log <file> — the tail, for out-of-order output like a build log
    local file="${1:-}"
    if [ -n "$file" ] && [ -f "$file" ]; then
        printf -- '--- %s (last 25 lines) ---\n' "$file" >&2
        tail -n 25 "$file" | sed 's/^/      /' >&2
    fi
    return 0
}
