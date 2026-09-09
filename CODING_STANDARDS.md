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

- clang-format per the repo's `.clang-format` (run the repo's format target if
  present). Match existing naming conventions.
- Keep the naming/architecture conventions documented in this repo's
  CLAUDE.md / README.

## Definition of done (agent checklist)

- [ ] Zero-warning build (see Tooling status — `-Werror` where wired)
- [ ] All tests pass
- [ ] Tests pass under ASan+UBSan
- [ ] clang-tidy — no NEW findings vs baseline (where tidy is configured)
- [ ] No raw owning pointers / `new` / `reinterpret_cast` introduced
- [ ] Test written first (RED) for every behavior change or bug fix

## Tooling status (this repo, as of 2026-09-08)

- Warnings: compile flags on `jt_core`/`jt*` targets (see CMakeLists.txt).
  `-Werror` pending a verified zero-warning baseline.
- Tests: build with `-DJT_BUILD_TESTS=ON`, run the produced test binary.
- Sanitizers: `cmake -B build-asan -DJT_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS=
  "-fsanitize=address,undefined -fno-omit-frame-pointer"` then build + run tests.

## Upstream reference

https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines — the Profiles
chapter (Type / Bounds / Lifetime) is the conceptual core.
