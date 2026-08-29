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

Deferred by design from the fix pass — none were in scope for batches 1-2:

4. **No `--` end-of-options.** Flag-looking values unrepresentable:
   `jtSet /cmd --help` triggers help; unknown flags silently become
   positionals in most tools (only jtFilter reports them).
6. **Transitive-include fragility.** jt_get.cpp, jt_filter.cpp, jt_sort.cpp
   use jt::fail/jt::run_cli without including jt/errors.hpp.
8. (folded into A.4 above)
9. **`fail` duplicates `Error::format`** — DRY.
10. **Two Args structs** — common.hpp's `Args` appears vestigial; verify and
    remove.
13. (folded into A.3 above — stdout/stream-state checking)
14. **Help is one line per tool**; flags undocumented in-tool.

## Suggested batching

- **Batch C (quick wins):** A.1, A.2, B.6, B.9, B.10 — mechanical, low risk,
  one careful session.
- **Batch D (semantics):** B.4 (`--` end-of-options touches every main —
  design it once in args.hpp first), A.5, B.14.
- **Batch E (robustness):** A.3 — needs pipe/EOF test scaffolding.

Working method that worked: strict TDD per item (RED → watch fail → minimal
GREEN → full suite → commit naming the item), as in docs/fix-brief-batch1.md
and batch2.
