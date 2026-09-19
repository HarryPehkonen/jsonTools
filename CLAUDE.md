# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

jsonTools is a set of pipe-composable JSON mutation utilities — a thin,
safety-first command-line layer over [JSOM](https://github.com/HarryPehkonen/JSOM).
Each tool is a `jt_<verb>` binary (`src/jt_<tool>.cpp`) exposing one verb
(get, set, remove, ...); all tools share the `jt_core` static library.

**Status: pre-release (v0.x).** Behavior may change without notice; pin
expectations to the tested version. README is the authoritative verb/flag
reference.

## Build & Test

```bash
cmake -B build                 # builds jt_core + all jt tools
cmake --build build -j$(nproc)

# Tests (OFF by default)
cmake -B build -DJT_BUILD_TESTS=ON
cmake --build build -j$(nproc)
./build/jt_tests               # exact binary name per tests/CMakeLists.txt
```

### Local CI (the gates, run by the hooks)

`tools/ci.sh` runs every gate — `tree format build tests cli asan tidy wire
version pristine` — and is the single definition of "the gate passed":
`.githooks/pre-commit` runs `tree build tests` (fast), `.githooks/pre-push` runs
the whole set and additionally requires a clean worktree. Enable once per clone
with `git config core.hooksPath .githooks`. Per-machine settings live in `.ci.env`
(gitignored; see `.ci.env.example`); stage output goes to `.ci-logs/`. Run it
directly while working: `tools/ci.sh build tests`, `tools/ci.sh tidy`,
`tools/ci.sh --list`. The `wire` stage is the one to know when adding a tool: a
new `src/jt_<verb>.cpp` must also reach the CMake build list, the install list,
the tests' `add_dependencies`, a test that spawns it, and README.

## C++ Standards (MANDATORY)

All C++ work in this repo MUST follow `CODING_STANDARDS.md` — modern C++17 in
the spirit of the C++ Core Guidelines (Type/Bounds/Lifetime profiles),
exceptions allowed for error handling. Binding for every agent run.

Gates before any change is done:

1. Zero-warning build (flags on `jt_core`/`jt*` targets; `-Werror` wiring per
   Tooling status in CODING_STANDARDS.md).
2. Tests pass (build with `-DJT_BUILD_TESTS=ON` and run the test binary); TDD
   (failing test first) for every behavior change or bug fix.
3. Sanitizer gate: rebuild with `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined
   -fno-omit-frame-pointer"` and run the tests — clean under ASan+UBSan.
4. Never introduce raw owning pointers, `new`/`delete`, or
   `reinterpret_cast`/C-style casts.
5. If a build fails on a pre-existing warning, fix the warning (small, targeted
   change) rather than weakening the flags.
6. `tools/ci.sh` green — items 1-4 above are stages of it (`build`/`tests`/`asan`),
   plus the repo invariants no general tool knows: every file committed or ignored,
   formatting, clang-tidy, every tool wired into CMake/tests/README, one version
   string, and a pristine build of HEAD. See "Local CI" above.
