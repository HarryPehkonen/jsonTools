You are fixing issues in this C++17 repo (jsonTools). Read FIX_ME.md first —
it is the source of truth. THIS PASS covers Batch C only: items A.1, A.2,
B.6, B.9, B.10. Do NOT touch Batch D (A.5, B.4, B.14) or Batch E (A.3).

## WORKING METHOD: strict TDD — mandatory

For EACH item, one at a time (vertical tracer bullets):
1. RED — a GoogleTest case (or a test that can pass) capturing the correct
   behavior/state; it must fail now where testable. For pure refactors
   (A.1, B.9, B.10), where behavior must NOT change, the "test" is the
   existing suite: refactor with zero test changes and the suite green.
2. WATCH — build, run the specific test (confirm RED for the right reason)
   or the full suite (confirm green throughout a pure refactor).
3. GREEN — minimal change.
4. Full suite green (cmake --build build -j && ./build/jt_tests && bash
   smoke.sh), then COMMIT naming the FIX_ME item.

Rules:
- WORK ONLY IN THIS REPO. JSOM is read-only reference
  (/home/harri/hermes-workspace/JSOM/**): read, never modify.
- Preserve: RFC 6901 strictness, error format, exit-non-zero-on-error, no
  silent coercion, all current error message TEXT (tests pin them).
- Update REQUIREMENTS.md/TECHNICAL_DETAILS.md where structure changes
  (not for pure internal refactors).
- Do not push.

## The items

A.1 — The array-bounds hint block ("the array at X has N elements
      (indexes 0-M)" with empty/append variants) is copy-pasted three
      times: require_at, require_parent, create_object_path in
      src/paths.cpp. Extract ONE helper that builds the hint string; the
      three call sites pass their specifics. Pure refactor: identical
      messages, suite green with no test edits.

A.2 — Version "0.1.0" is hardcoded in src/args.cpp (take_global,
      --version). Create a single source of truth (e.g. include/jt/
      version.hpp with JT_VERSION) and use it from args.cpp. RED where
      possible: a test can include version.hpp and pin that --version
      output contains it (spawn jtSet --version like test_mains.cpp does).

B.6 — Transitive-include fragility: src/jt_get.cpp, src/jt_filter.cpp,
      src/jt_sort.cpp use jt::fail / jt::run_cli without including
      jt/errors.hpp (or common.hpp). Add the explicit includes.

B.9 — jt::fail (src/errors.cpp) duplicates jt::Error::format logic.
      Make fail build its message via Error::format (or share one
      formatting function) so there is exactly one place that renders
      "Error at <path>: <problem>. <suggestion>".

B.10 — include/jt/common.hpp has an `Args` struct that looks vestigial
      (superseded by GlobalArgs in args.hpp?). Verify no user, delete if
      truly dead (a search across src/ and tests/ plus a full build is
      the verification; if it IS used, document the use and leave it,
      noting that in the commit).
