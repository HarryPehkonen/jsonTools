You are fixing issues found in a code review of this C++17 repo (jsonTools:
pipe-composable JSON mutation CLI over JSOM; strict RFC 6901 pointers;
error model "Error at <path>: <problem>. <suggestion>").

Read docs/REVIEW-glm53-20260828.md first — it is the source of truth for
the issues. THIS PASS covers issues 1, 2, 3, 5, 7, 12 (correctness and the
error-quality headline). Do NOT touch issues 4, 6, 8-14 in this pass.

The issues:
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

Rules:
- Preserve: RFC 6901 strictness, error format, exit-non-zero-on-error, no
  silent type coercion. Pre-release, but don't break existing behavior
  silently — tests encode it.
- WORK ONLY IN THIS REPO. JSOM is a separate repo (read-only reference —
  you may READ /home/harri/hermes-workspace/JSOM/** to understand pointer
  semantics, but do NOT modify it). If a fix seems to require changing
  JSOM, implement what you can in jsonTools and note the JSOM gap in the
  commit message instead.
- Add/extend GoogleTest cases for each fixed behavior.
- Update REQUIREMENTS.md/TECHNICAL_DETAILS.md where observable semantics
  change.
- Build: cmake --build build -j  (build dir already configured with tests)
  then run ./build/jt_tests AND ./smoke.sh. Everything green before commit.
- Commit per logical fix (or per small group) with clear messages. Do not
  push.
