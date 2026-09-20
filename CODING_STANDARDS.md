# jsonTools — C++ Coding Standards (MANDATORY for all agents and humans)

This file is the binding standard for all C++ work in this repository. It
implements the *C++ Core Guidelines* (Type / Bounds / Lifetime profiles) in a
form that is machine-checkable and agent-followable. Exceptions are ALLOWED
for error handling — this is not MISRA/JSF.

Reference: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines

## Hard rules (no exceptions)

1. **C++17 minimum.** No C-style code in new work.
2. **No raw owning pointers.** Use `std::unique_ptr` / `std::shared_ptr` and
   RAII. A raw pointer (or reference) is a *non-owning view* only.
3. **No `new` / `delete`**, no `malloc` / `free` in project code.
4. **Views over ranges:** `std::span` for contiguous buffers, `std::string_view`
   for read-only string parameters, instead of pointer+length.
5. **No `reinterpret_cast` or C-style casts.** `const_cast` only with a comment.
6. **No undefined behavior:** no reliance on signed overflow, no out-of-bounds
   access (use `.at()` or checked access), nothing read uninitialized —
   initialize every variable at declaration.
7. **`[[nodiscard]]`** on accessors and pure functions. If a call site must
   discard the value, cast to `(void)` with a comment.
8. **Exceptions are for error handling only**, never control flow. RAII
   guarantees cleanup on throw. Throw the project's documented exception type,
   never built-ins.
9. **No global mutable state.** Threaded code must be race-free: mutex/atomic,
   prefer immutable data.
10. **Never keep iterators or references across container mutation.** Re-fetch
    after any mutating call (the Lifetime profile's core rule).
11. **Zero warnings.** Project targets compile with
    `-Wall -Wextra -Wpedantic -Werror` where wired (see Tooling status).
12. **TDD.** Write the failing test first, watch it fail, implement, watch it
    pass. Bug fixes too: failing test that isolates the bug → fix → green.
13. **Sanitizer gate:** all tests must pass under ASan+UBSan before a change is
    done (see Tooling status for this repo's exact command).

## Style

- clang-format per the repo's `.clang-format` (the same file as JSOM and
  Computo: LLVM base, 4-space indent, 100 columns). Check with
  `tools/ci.sh format`; apply with
  `clang-format -i $(git ls-files 'include/jt/*.hpp' 'src/*.cpp' 'tests/*.cpp')`.
- Match existing naming conventions.
- Keep the naming/architecture conventions documented in this repo's
  CLAUDE.md / README.

## Definition of done (agent checklist)

- [ ] Zero-warning build (see Tooling status — `-Werror` is wired on every
      project target)
- [ ] All tests pass
- [ ] Tests pass under ASan+UBSan
- [ ] clang-tidy — zero findings (`tools/ci.sh tidy`)
- [ ] No raw owning pointers / `new` / `reinterpret_cast` introduced
- [ ] Test written first (RED) for every behavior change or bug fix
- [ ] `tools/ci.sh` green (the whole list above, plus the repo invariants)

## Tooling status (this repo, as of 2026-09-19)

- Warnings: `-Wall -Wextra -Wpedantic -Werror` on `jt_core`, every `jt*` tool
  and `jt_tests`; verified zero-warning on a from-scratch build.
- Formatting: `.clang-format` is committed and enforced (`tools/ci.sh format`).
- Static analysis: `.clang-tidy` — value-only checks (`bugprone-*`,
  `performance-*`), scoped to this repo's own files so the sibling JSOM's
  headers are not reported as our findings. Enforced with zero findings
  (`tools/ci.sh tidy`).
- Tests: `cmake -S . -B build -DJT_BUILD_TESTS=ON -DJSOM_SOURCE_DIR=/path/to/JSOM`,
  `cmake --build build -j`, `./build/jt_tests` — 216 tests.
- Sanitizers: `tools/ci.sh asan` configures `build-asan` with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` and runs the same suite.
- Everything above runs through one script, `tools/ci.sh`, called by hand and by
  the hook in `.githooks/` (see README, "Local CI"). It also gates the
  repo-specific invariants that no general tool knows about: every file
  committed or ignored, every tool wired into CMake/tests/README, one version
  string, and a pristine build of `HEAD`.

## Upstream reference

https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines — the Profiles
chapter (Type / Bounds / Lifetime) is the conceptual core.
