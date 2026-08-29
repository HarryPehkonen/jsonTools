You are fixing issues found in a code review of this C++17 repo (jsonTools:
pipe-composable JSON mutation CLI over JSOM; strict RFC 6901 pointers;
error model "Error at <path>: <problem>. <suggestion>").

Read docs/REVIEW-glm53-20260828.md first — it is the source of truth.
THIS PASS covers issues 1, 2, 3, 5, 7, 12 (correctness and the
error-quality headline). Do NOT touch issues 4, 6, 8-14 in this pass.

## WORKING METHOD: strict TDD — this is mandatory

For EACH issue, in order, one at a time (vertical tracer bullets — never
"write all the tests, then all the fixes"):

1. RED — write a GoogleTest case in tests/ that captures the CORRECT
   behavior for one concrete aspect of the issue. It must fail against the
   current code.
2. WATCH IT FAIL — build and run that test; confirm it fails for the
   expected reason (bug present, not a typo/compile error). If it passes
   immediately, your test tests the wrong thing — fix the test first.
3. GREEN — make the MINIMAL code change that passes the test. Nothing
   extra; don't refactor neighboring code yet.
4. WATCH IT PASS — run the specific test, then the FULL suite
   (./build/jt_tests) for regressions. Fix regressions before continuing.
5. Next aspect of the same issue: new RED test, repeat. When the issue is
   fully covered, refactor if needed (tests stay green), then COMMIT with a
   clear message naming the review issue number.

Rules:
- WORK ONLY IN THIS REPO. JSOM is a separate repo (read-only reference —
  you may READ /home/harri/hermes-workspace/JSOM/** to understand pointer
  semantics, but do NOT modify it). If a fix seems to require changing
  JSOM, implement what you can in jsonTools and note the JSOM gap in the
  commit message instead.
- Preserve: RFC 6901 strictness, error format, exit-non-zero-on-error, no
  silent type coercion. Pre-release, but don't break existing behavior
  silently — tests encode it.
- If an existing test must change because behavior is INTENTIONALLY
  changing, say so explicitly in the commit message.
- Update REQUIREMENTS.md/TECHNICAL_DETAILS.md where observable semantics
  change; keep README examples verified (run them).
- Build: cmake --build build -j  (build dir already configured with tests)
  then ./build/jt_tests AND ./smoke.sh. Everything green before each commit.
- Do not push.

## The issues

1. require_parent checks existence, not container-ness — a scalar parent
   passes; an array parent passes without index-range validation. Make it
   honor its documented contract.
2. create_object_path refuses to descend into arrays entirely; -p must not
   break setting into EXISTING array elements, and the error hint must
   distinguish "array exists (append via /-)" from "array missing". The
   hint's example command must actually work for the case it suggests.
3. find()==nullptr conflates three failures into "path not found": missing
   key, index out of range, tunneling through a scalar. Classify precisely
   in the error (the pointer-walk to do this already exists in suggest_for).
   This is the headline feature — take care with it.
5. jtSort must validate that it received non-empty keys (jtFilter already
   does); decide and document the bare-jtSort behavior.
7. nearest_key must escape the suggested key (~/) exactly like suggest_for
   does in paths.cpp.
12. Reject empty-string positional args in the tool mains (empty path as
    root via jtSet "" is a shell-quoting footgun) — unless a verb
    legitimately needs it; jtGet / (root) must keep working.
