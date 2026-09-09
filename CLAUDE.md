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
