# FIX_ME.md — jsonTools open issues

Independent code review by GPT-5.4 (reading the code directly), 2026-08-29,
after the GLM-5.3 review-and-fix pass (commits d9f4178..cc37e7c). Everything
below is what a second reviewer still finds. Small, ordered, and honest about
priority.

## A. New findings (this review)

1. **Array-bounds hint block is copy-pasted three times.**
   `require_at`, `require_parent`, and `create_object_path` each build "the
   array at X has N elements (indexes 0-M)" with the same empty/append
   variants (src/paths.cpp ~lines 112-123, 166-181, 219-235). The review fix
   for issue 3/2 created this DRY debt. Extract one helper returning the hint
   string. Tests: existing coverage already pins the messages — a refactor
   with the suite green is the whole verification.

2. **Version "0.1.0" is hardcoded in `src/args.cpp`** (take_global,
   --version output) and will drift from any future pyproject/doc bumps.
   One `version.hpp` generated or hand-maintained, included from args.cpp.

3. **I/O failure paths untested:**
   - `read_stdin` (src/common.cpp) doesn't check stream state after reading —
     a mid-stream EOF (truncated pipe) reads as "invalid JSON" rather than a
     read error.
   - `write_stdout` doesn't check write success (broken pipe downstream in a
     real pipeline).
   Low-probability in practice; these are the paths that bite in pipes, so
   worth one test each (a closed-fd write, a truncated input).

4. **`option_number` uses `stoll` + exceptions** (src/args.cpp).
   `std::from_chars` is cleaner for C++17, and the current code accepts "+5".
   Cosmetic; do it whenever args.cpp is touched anyway.

5. **Levenshtein threshold fixed at ≤2 regardless of key length**
   (src/errors.cpp `nearest_candidate`); ties resolved by iteration order.
   (Also review item 11 — listed here so the list is one place.) Consider
   distance ≤ max(1, len/4), and documenting tie-breaking as "first key in
   insertion order".

## B. Remaining GLM-review items (docs/REVIEW-glm53-20260828.md)

**Status of batches D and E: PARKED (2026-08-29, Harri's decision).**
Reopen an item ONLY when a real-world failing case appears — no fix without
evidence from actual use. "It might be nice" is not a reopening reason.
Parked items:

4. **No `--` end-of-options.** (Real case that would reopen: needing to set
   a *value* that begins with `--`.)
6. ~~Transitive-include fragility~~ (verified already fixed; pinned by
   test_includes.cpp)
8. (folded into A.4 above)
9. ~~`fail` duplicates `Error::format`~~ (fixed, B.9)
10. ~~Two Args structs~~ (fixed, B.10)
13. (folded into A.3 above — stdout/stream-state checking)
14. **Help is one line per tool** — the one-liner is a feature for pipe
    tools; reopen only if a real user is actually confused.

Also parked, from section A:

- **A.5** Levenshtein threshold — distance ≤2 works on real keys; reopen
  when a real typo goes unsuggested.
- **A.3** I/O failure paths — default SIGPIPE death is Unix-correct for a
  pipe tool; truncated-stdin test is marginal.

## Done

- Batch A.1, A.2, B.6, B.9, B.10 — fixed (commits c84030a..7f52e31,
  216/216 green, from-scratch build clean).
- GLM-review issues 1, 2, 3, 5, 7, 12 — fixed (commits d9f4178..cc37e7c,
  strict TDD, 212 tests at the time).


## Decision

Shipped as v0.1.0 (2026-08-29). The tool set is complete for its purpose:
one mutation per binary, composable, strict errors, 216 tests green,
from-scratch build clean. Remaining items are parked, not owed — the
backlog's standing rule is the project's oldest one: **no fix without a
failing real-world case.**
