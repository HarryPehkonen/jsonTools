You are continuing a fix pass on this C++17 repo (jsonTools). Issues 1 and 2
from docs/REVIEW-glm53-20260828.md are DONE and verified (commits d9f4178,
96bf692). THIS PASS covers the remaining issues 3, 5, 7, 12. Do NOT touch
anything else.

## WORKING METHOD: strict TDD — mandatory, same as before

For EACH issue, one at a time (vertical tracer bullets):
1. RED — GoogleTest case capturing correct behavior; it must fail now.
2. WATCH IT FAIL — build, run it, confirm it fails for the right reason.
3. GREEN — minimal code change to pass.
4. WATCH IT PASS — the specific test, then the FULL suite (./build/jt_tests).
5. Next aspect → new RED test. When the issue is covered, refactor (tests
   stay green), then COMMIT naming the review issue number.

Rules:
- WORK ONLY IN THIS REPO. You may READ /home/harri/hermes-workspace/JSOM/**
  (read-only reference for pointer semantics); do NOT modify it.
- Preserve: RFC 6901 strictness, error format, exit-non-zero-on-error, no
  silent type coercion.
- Build: cmake --build build -j, then ./build/jt_tests AND bash smoke.sh.
  Green before each commit. Do not push.

## The issues

3. **Error classification (the headline feature).** find()==nullptr currently
   conflates three distinct failures into "path not found": (a) missing key,
   (b) index out of range, (c) tunneling through a scalar. The pointer-walk
   needed to classify already exists (see suggest_for / the new
   require_parent work in paths.cpp). Classify precisely so each case gets
   its own message. Note: issue 1 already fixed classification at the LEAF
   parent level (require_parent); issue 3 is about the INTERMEDIATE walk and
   about jtGet/jtMove/jtCopy/jtRemove style lookups that go through find().
   Test each of the three cases separately.
5. jtSort must validate non-empty keys (jtFilter already does); decide and
   document the bare-jtSort behavior.
7. nearest_key must escape the suggested key (~/) exactly like suggest_for
   does in paths.cpp.
12. Reject empty-string positional args in the tool mains (jtSet "" as a
    shell-quoting footgun) — unless a verb legitimately needs it; jtGet /
    (root) must keep working.
