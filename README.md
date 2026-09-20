# jsonTools

> **Status: pre-release.** jsonTools is at version 0.x and actively evolving.
> Nothing about the current behavior is guaranteed — verb semantics, flags,
> error messages, and even the verb set may change without notice or
> backwards compatibility. If you script against it today, pin your
> expectations to the exact version you tested against.

Pipe-composable JSON mutation utilities. A thin, safety-first command-line
layer over [JSOM](https://github.com/HarryPehkonen/JSOM).

jsonTools is inspired by tabTools: **one mutation per command**, composed with
Unix pipes. It consumes and produces JSON on stdin/stdout, so the tools chain
and interleave with `jq` or anything else that speaks JSON.

## Why

A friendlier, more debuggable front-end to the kind of JSON transforms that
otherwise live in one-shot scripts. Because each command is a single mutation,
you can stop the pipe at any `|` and inspect the intermediate state — the
thing a monolithic script won't let you do.

## Build

```bash
cmake -S . -B build -DJSOM_SOURCE_DIR=/path/to/JSOM   # or omit to FetchContent JSOM
cmake --build build -j
```

Binaries are `build/jtNew`, `build/jtSet`, … (case-sensitive). Tests (optional):

```bash
cmake -S . -B build -DJSOM_SOURCE_DIR=/path/to/JSOM -DJT_BUILD_TESTS=ON
cmake --build build -j
./build/jt_tests
```

## Local CI (the commit gate)

Formatting, linting, tests, sanitizers and repo invariants are enforced by **one
script**, `tools/ci.sh`, and by two git hooks that call it. No hosted CI, no
network: the same script runs by hand, on commit, and on push.

```bash
git config core.hooksPath .githooks   # once per clone: arms the gate on push
tools/ci.sh                           # every stage, by hand
tools/ci.sh build tests               # just these stages, in the order given
tools/ci.sh --list                    # what the stages are
```

| stage | what it proves |
| --- | --- |
| `tree` | every file is committed or ignored (untracked **and** unignored fails), `.gitignore` still covers what the gate itself creates, no tracked file is ignored |
| `format` | the C++ files **this branch touched** match the repo `.clang-format` (a whole-tree check drowns in drift nobody edited, and gets muted) |
| `build` | CMake configure + build with zero warnings (`-Werror`) |
| `tests` | `./build/jt_tests` — 216 tests |
| `cli` | the README examples and the error contract, end to end through the real binaries |
| `asan` | the same suite under ASan + UBSan |
| `fuzz` | `build-fuzz/` (clang, coverage + ASan/UBSan) and `CI_FUZZ_SECONDS` (default 60) of libFuzzer over the pointer/argv parsing surface |
| `tidy` | clang-tidy over `src/`, `tests/` and `fuzz/`, zero findings |
| `wire` | every tool is built, installed, depended on by the tests, exercised, and documented — the guard for the next tool you add |
| `version` | `include/jt/version.hpp` == CMake `VERSION` == what every binary prints for `--version` |
| `pristine` | `git archive HEAD` configures, builds and tests in a temp dir: the **committed** tree is complete |

`.githooks/pre-commit` runs `tree build tests` — 1 s when nothing has changed, ~3 s
after a one-file edit (incremental build + the 216 tests) — and `.githooks/pre-push`
runs the whole gate with `--require-clean`. So every commit is a tree that builds
and passes, and publishing additionally checks format, lint, sanitizers, the CLI
contract, the fuzz smoke and a from-scratch build of the committed tree. Deliberate
bypass is `git commit --no-verify` / `git push --no-verify`. Enabling the hooks is
per clone, not per repo — a fresh clone has no gate until that one `git config`
line runs.

Every run ends with one unambiguous line — `GATE PASSED` or `GATE FAILED` — and a
non-zero exit when it failed. A failure also names every requested stage that never
ran, as `BLOCK <stage> (did not run: the run stopped at <stage>)`, because a stage
that did not run must never read as one that passed.

Machine-local settings live in `.ci.env` (gitignored; copy `.ci.env.example`).
The one that matters most is `CI_JSOM_DIR`, the JSOM checkout to build against:

- default: the sibling `../JSOM` when it exists, else CMake's `FetchContent`
  (which needs the network and floats on JSOM's `main`);
- every run prints the revision it built against, so a break coming from JSOM is
  visible instead of mysterious.

Per-stage output is kept in `.ci-logs/` (gitignored); on failure the tail is
printed and the path named.

### Fuzzing

JSOM fuzzes JSON text; jsonTools fuzzes what jsonTools owns — RFC 6901 pointer
strings, the traversal that turns one into a document position
(`require_at`/`require_parent`/`create_object_path`), the write verbs built on it
(`set` with and without `-p`, `copy`/`move` in all three `DestMode`s, `remove`)
and the argv helpers every tool shares. The harness is `fuzz/jt_fuzz.cpp`, its
corpus is `fuzz/seeds/` plus a dictionary of pointer tokens (`fuzz/jt.dict`).

```bash
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DJT_BUILD_FUZZING=ON \
      -DJSOM_SOURCE_DIR=/path/to/JSOM
cmake --build build-fuzz --target build_fuzzer
./build-fuzz/fuzz_jt build-fuzz/corpus fuzz/seeds \
      -dict=fuzz/jt.dict -artifact_prefix=build-fuzz/corpus/ -max_total_time=60

# replay a finding (the artifact you get from a failing fuzz stage)
./build-fuzz/fuzz_jt build-fuzz/corpus/crash-<hash>
```

The gate runs 60 seconds of that on every push (`tools/ci.sh fuzz`); the long
campaign is the nightly cron (`~/hermes-workspace/cron/fuzz_overnight.sh`), whose
findings are reported by the `fuzz-report` job. A crash found there becomes a
regression test in `tests/` — the same rule as any other bug fix.

## Install

One install target, two prefixes — no separate targets needed.

```bash
# Single-user (default; no sudo) → ~/.local/bin, ~/.local/lib, ~/.local/include
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --install build

# System-wide (needs sudo) → /usr/local/bin, /usr/local/lib, /usr/local/include
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
sudo cmake --install build
```

This installs the 15 CLI binaries, the static `libjt_core.a`, and the `jt/`
headers (for use as a library).

## The verbs

```
jtNew      jtFrom      jtSet      jtMove     jtCopy
jtRemove   jtGet       jtSelect   jtZip      jtSort
jtFilter   jtLen       jtType     jtKeys     jtValues
```

All paths are strict [RFC 6901 JSON Pointers](https://datatracker.ietf.org/doc/html/rfc6901).
Per-element verbs (`jtSort`, `jtFilter`) take element-relative keys (no leading
slash); document verbs take absolute pointers (leading slash).

See `REQUIREMENTS.md` for full semantics, and `TECHNICAL_DETAILS.md` for the
implementation mapping and the C++ library API.

## Examples

Each shows the command, then its output. Values passed to `jtSet` are JSON
literals, so strings are quoted and numbers/booleans/null are bare.

### Build an object

```bash
$ jtNew | jtSet /name '"harri"' | jtSet /age 48
{"age":48,"name":"harri"}
```

### Nested structure — three ways

```bash
# whole literal at once
$ jtNew '{"key1":{"key2":{"key3":"value"}}}'
{"key1":{"key2":{"key3":"value"}}}

# one mutation, -p creates the intermediate objects
$ jtNew | jtSet /key1/key2/key3 '"value"' -p
{"key1":{"key2":{"key3":"value"}}}

# explicit, one level per mutation
$ jtNew | jtSet /key1 '{}' | jtSet /key1/key2 '{}' | jtSet /key1/key2/key3 '"value"'
{"key1":{"key2":{"key3":"value"}}}
```

### Environment variables (note the quoting)

```bash
$ jtNew | jtSet /home "\"$HOME\""
{"home":"/home/harri"}

$ jtNew | jtSet /path/home "\"$HOME\"" -p
{"path":{"home":"/home/harri"}}
```

The `\"` keeps the double quotes as part of the JSON string while letting the
shell expand `$HOME`. Single quotes won't work — `'$HOME'` stays literal and
isn't valid JSON.

### Reshape (move a value)

```bash
$ echo '{"user":{"name":"harri"}}' | jtMove /user/name /name
{"name":"harri","user":{}}
```

### Append to an array (RFC 6902 `-` sentinel)

```bash
$ echo '{"items":["a","b"]}' | jtSet /items/- '"c"'
{"items":["a","b","c"]}
```

### Build an object from two arrays

```bash
$ echo '{"keys":["name","age"],"vals":["harri",48]}' | jtZip /keys /vals
{"age":48,"name":"harri"}
```

### Count a list (into a field, document intact)

```bash
$ echo '{"items":[1,2,3]}' | jtLen /items /count
{"count":3,"items":[1,2,3]}
```

### List an object's keys

```bash
$ echo '{"a":1,"b":2}' | jtKeys
["a","b"]
```

### Keys + values round-trip through jtZip

```bash
$ echo '{"o":{"a":1,"b":2}}' | jtKeys /o /ks | jtValues /o /vs | jtZip /ks /vs
{"a":1,"b":2}
```

(Form B keeps the document intact so introspection results can feed the next
step — `jtKeys`/`jtValues` write to a destination instead of replacing the
document. Here the object at `/o` is decomposed into keys and values, then
re-assembled by `jtZip`.)

### Filter → sort → project (a real chain)

```bash
$ echo '{"users":[{"name":"ada","age":36},{"name":"harri","age":48},{"name":"bob","age":51}]}' \
    | jtFilter /users age --gt 40 \
    | jtSort /users name \
    | jtSelect /users
{"users":[{"age":51,"name":"bob"},{"age":48,"name":"harri"}]}
```

### Errors — actionable, with typo hints

```bash
$ echo '{"name":"harri"}' | jtMove /nam /x
Error at /nam: path not found. did you mean /name?
```

## Errors

Errors are `Error at <path>: <problem>. <suggestion>` and exit non-zero —
including Levenshtein "did you mean" hints for typos. jsonTools never silently
coerces types or hides a missing path.
