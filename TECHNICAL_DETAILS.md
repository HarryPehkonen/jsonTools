# jsonTools — Technical Details

> Implementation-facing spec. Companion to `REQUIREMENTS.md`. Describes how
> each verb maps onto the JSOM library, the shared code layout, the CLI
> argument grammar, and the test strategy.

## 1. Dependencies and build

- **JSOM** is the only external dependency, consumed via CMake `FetchContent`
  or `add_subdirectory` (JSOM auto-disables its tests when built as a
  dependency — see its `JSOM_BUILD_TESTS` option).
- **C++17** minimum. **CMake** build, one top-level `CMakeLists.txt` for all
  `jt*` binaries (tests may use their own `CMakeLists.txt`).
- **Zero other deps** — no nlohmann/json, no CLI framework. Argument parsing
  is hand-rolled per tool (tabTools style: a small shared `args.hpp`).

### Directory layout

```
jsonTools/
├── CMakeLists.txt          # builds all jt* binaries + libjt
├── include/jt/             # shared headers
│   ├── common.hpp          #   read_stdin / write_stdout / error macros
│   ├── args.hpp            #   shared arg parsing helpers
│   └── errors.hpp          #   Error at <path>: <problem>. <suggestion>
├── src/
│   ├── common.cpp          #   stdin/stdout I/O, pretty-print wiring
│   ├── errors.cpp          #   error formatting + Levenshtein hint
│   ├── jt_new.cpp
│   ├── jt_from.cpp
│   ├── jt_set.cpp
│   ├── jt_move.cpp
│   ├── jt_copy.cpp
│   ├── jt_remove.cpp
│   ├── jt_get.cpp
│   ├── jt_select.cpp
│   ├── jt_zip.cpp
│   ├── jt_sort.cpp
│   ├── jt_filter.cpp
│   └── jt_len.cpp
└── tests/
    ├── CMakeLists.txt      # Google Test (matches JSOM/Computo convention)
    └── test_*.cpp
```

## 2. Shared core (`libjt`)

### 2.1 The common I/O contract

Every tool follows the identical shape:

```cpp
int main(int argc, char* argv[]) {
    jt::Args args = jt::parse(argc, argv, usage);
    JsonDocument doc = jt::read_stdin(args);   // parse whole stdin, or error
    jt::apply(args, doc);                       // the ONE mutation
    jt::write_stdout(doc, args);                // compact (or --pretty)
    return 0;
}
```

- `jt::read_stdin` reads all of stdin, `parse_document` on it. Parse failure →
  `Error at <input>: <JSOM's message>` on stderr, exit non-zero, **nothing on
  stdout**.
- `jt::write_stdout` emits via `JsonFormatter` — `compact` preset by default,
  `pretty` (or another preset) when `--pretty` is passed. The formatter lives
  in shared code; each tool only passes the flag through.

### 2.2 Error model (`errors.hpp`)

```cpp
[[noreturn]] void fail(const std::string& path,
                       const std::string& problem,
                       const std::string& suggestion = "");
```

Formats exactly `Error at <path>: <problem>. <suggestion>` to stderr, then
`exit(1)`. The `<suggestion>` is supplied by the tool (e.g. a Levenshtein
"did you mean" hint against real sibling keys when a lookup fails).

A helper `nearest_key(doc, path, candidate)` computes the closest existing
key by edit distance and returns a suggestion string — shared, so `jtGet`,
`jtMove`, `jtRemove`, `jtFilter` all get typo hints for free.

### 2.3 Argument conventions

- Positional args are JSON Pointers; options are `--kebab-case`.
- Values that are JSON **literals** are passed as JSON text and parsed with
  `parse_document` — never treated as bare strings. (So `jtSet /x '"hi"'`
  sets a string; `jtSet /x '18'` sets a number; `jtSet /x 'null'` sets null.)
- The optional `[listPath]` convention: a verb operating on a container takes
  an optional leading pointer defaulting to `/`.

## 3. Per-verb implementation notes

### 3.1 `jtNew`
```
jtNew [literal]
```
No stdin read. Emits `parse_document(literal)` if given, else `{}` (an empty
object — the `--array` flag or a literal `[]` selects an empty array).

### 3.2 `jtFrom`
```
jtFrom <file> [--check-only] [--max-depth N] [--max-size N] [--warn-duplicates]
```
Reads `<file>`, `parse_document` with `JsonParseOptions`. Validations before
emit: syntax (mandatory), depth cap (walk the tree, fail past `N`), size cap,
optional duplicate-key warning (JSOM parse options / a post-pass). Emits the
document (or nothing + exit 0 under `--check-only`).

### 3.3 `jtSet`
```
jtSet <path> <literal> [-p]
```
Resolve the path with `require_parent` (missing intermediate → **error
unless `-p`**). With `-p`, `create_object_path` first walks the pointer's
intermediate segments: it creates missing ones as **objects** (a numeric
segment becomes an object key — `-p` never creates or grows an array) and
descends into existing array elements by in-range index. The container that
receives the leaf must be able to hold it: a scalar parent errors ("cannot
put a value inside a `<type>`"), and an array parent accepts only an
in-range index or `-` — an out-of-range index errors ("index N is out of
range", with the array's actual bounds and an append hint that actually
unlocks the next index) **instead of null-padding**, and a non-index leaf
errors ("'X' is not an array index"). Then `set` the literal at the leaf.

- `-p` is the `mkdir -p` analog: without it, a missing parent is an error.
- `-p` never grows arrays; use the `-` append sentinel `jtSet /items/- ...`
  for array growth. An intermediate `-` errors ("'-' is only valid as the
  final segment").
- The container guard runs before `set_at`, so these cases surface as jt
  errors with the offending value named, not as a wrapped
  `JsonPointerTypeException`.

### 3.4 `jtMove` / `jtCopy`
```
jtMove <from> <to> [--if-not-set | --replace]
jtCopy <from> <to> [--if-not-set | --replace]
```
Read the value at `<from>` (error if missing). For `<to>`:
- **default** — write unconditionally (overwrite).
- **`--if-not-set`** — write only if `<to>` is currently absent.
- **`--replace`** — write only if `<to>` already exists (never create).

`jtMove` additionally removes `<from>` after a successful write. `jtCopy`
retains `<from>`. A move where `<to>` is a child of `<from>` is an error
(cycle). `<to>` lands through `set` (§3.3), so the parent-container guard
applies to destinations too: an out-of-range array index errors instead of
null-padding.

### 3.5 `jtRemove`
```
jtRemove <path>
```
Delete `<path>`. Missing → error with nearest-key hint.

### 3.6 `jtGet`
```
jtGet <path> [--default <literal>]
```
Navigate to `<path>`. Present → emit that value as the whole document.
Missing → `--default <literal>` (parsed as JSON) if given, else error.

### 3.7 `jtSelect`
```
jtSelect <path1> [<path2> ...]
```
Build a fresh object. For each path, navigate to the value and insert it under
its **leaf key** (flattening — `/user/name` → key `name`). Duplicate leaf keys
→ error. (Consistent with the safety-first rule: no silent overwrite.)

### 3.8 `jtZip`
```
jtZip <keysPath> <valuesPath>
```
Resolve both paths (must be arrays). Lengths differ → error. Build an object
`{ keys[0]: values[0], ... }`. Duplicate keys → error by default; `--overwrite`
switches to last-wins.

### 3.9 `jtSort`
```
jtSort [<listPath>] <key1> [--desc-for <key2> <key3>]
```
Resolve the list (default `/`). Sort is **stable**. If keys are given, each
element must be an object; sort by successive keys, each `--desc-for` key in
reverse — `--desc-for` applies to the **next single key only** (repeat it for
multiple descending keys). No keys → sort by element natural type: numbers
numerically, strings lexically, **mixed types → error**. (Future: a
`--js-coerce` flag enabling JavaScript-style comparison.)

### 3.10 `jtFilter`
```
jtFilter [<listPath>] <keyPath> --op <literal>
```
Resolve the list. Keep elements where `element[<keyPath>]` (relative,
no leading slash) satisfies the operator. Operators: `--eq --ne --gt --ge
--lt --le`. The literal is parsed as JSON, so `--eq '"bob"'` is string
equality and `--gt 18` is numeric. An element **missing** the key is dropped
silently (fails the predicate).

### 3.11 `jtLen`
```
jtLen <listPath> <destPath>
```
Resolve `<listPath>` (must be an array or object). Compute its length. `set`
that number at `<destPath>`, **creating the destination leaf** if absent
(consistent with `jtSet`'s leaf-creation; only a missing *intermediate* errors
unless `-p`). Document otherwise intact. (Form B: read length, write to a
field.)

## 4. JSOM integration points (grounded)

- **Parse**: `parse_document(str, JsonParseOptions)` → `JsonDocument`.
- **Navigate**: `NavigationEngine::navigate_with_cache(&doc, pointer, cache)`
  → `NavigationResult` (has `found`, the node, `steps_navigated`). Missing →
  `JsonPointerNotFoundException` (subclass of `JsonPointerException`).
- **Mutate**: `JsonDocument::set(key, value)`, `push_back(value)`, `contains(key)`,
  `keys()`, `items()`, `operator[]` / `at()`.
- **Type checks**: `is_object() / is_array() / is_number() / is_string() / …`.
- **Format**: `JsonFormatter` with `FormatPresets::compact / pretty / …`.
- **Pointer helpers**: `JsonPointer::get_parent`, `get_last_segment`,
  `is_array_index` — used by `jtSet` (`get_parent`), `jtSelect` (leaf key =
  `get_last_segment`), `jtSort`/`jtFilter` (relative sub-paths).

A single `PathCache` may be constructed per document and threaded through the
navigation calls to reuse JSOM's prefix caching (matters for `jtSort`'s
repeated key lookups and `jtFilter`'s per-element navigation).

## 5. CLI grammar (summary)

```
jtNew      [literal]
jtFrom     <file> [--check-only] [--max-depth N] [--max-size N] [--warn-duplicates]
jtSet      <path> <literal> [-p]
jtMove     <from> <to> [--if-not-set | --replace]
jtCopy     <from> <to> [--if-not-set | --replace]
jtRemove   <path>
jtGet      <path> [--default <literal>]
jtSelect   <path> [<path> ...]
jtZip      <keysPath> <valuesPath>
jtSort     [<listPath>] <key> [--desc-for <key> ...]
jtFilter   [<listPath>] <keyPath> --op <literal>
jtLen      <listPath> <destPath>

Global (all tools): --pretty  --help  --version
```

## 6. Testing strategy

Google Test (matches JSOM + Computo conventions). Per-verb test files plus
shared tests:

- **Round-trip**: `jtNew | jtSet | jtGet` reconstructs the expected doc.
- **Error cases**: missing path, type mismatch, `--if-not-set`/`--replace`
  edge cases, `jtZip` length mismatch, `jtSort` mixed-type, `jtSelect` dup
  key, `jtSet` without `-p` on missing parent.
- **Golden JSON**: a small corpus of input/command/expected-output triples
  (the "66+ tested examples" pattern from Computo).
- **Property tests**: `jtMove` then `jtMove` back (round-trip identity);
  `jtFilter` output length ≤ input length; `jtLen` value == `keys().size()`.

## 8. Library API (in-process use)

jsonTools is designed to be usable **both** as a CLI suite and as a C++ library
(`#include <jt/jt.hpp>`, link `jt_core` + JSOM). The verbs are the shared
functions; the CLI `main()`s are thin shims over them.

### 8.1 Structure discipline (function-first)

Each verb's logic lives in the library, **not** in `main()`. The CLI source is
an arg-parser + one call:

```cpp
// jt/set.hpp — the library
namespace jt {
  JsonDocument set(JsonDocument doc, const std::string& path,
                   const JsonDocument& literal, bool mkdir_p = false);
}

// jt_set.cpp — the CLI shim
int main(int argc, char* argv[]) {
  ... parse args ...
  JsonDocument doc = jt::read_stdin();
  doc = jt::set(std::move(doc), path, literal, mkdir_p);
  jt::write_stdout(doc, pretty);
}
```

This mirrors the pipe philosophy in code: each verb is a function taking a
document (by value, moved) and returning the mutated document, so callers
compose exactly like a shell pipeline:

```cpp
auto out = jt::set(jt::filter(jt::move(doc, "/a", "/b"), "age", jt::gt(18)),
                   "/x", 42);
```

### 8.2 Error model: throw vs exit (split)

The library must never `exit()` the host program. So:

- **Library** throws `jt::Error` — an exception carrying `path`, `problem`,
  and optional `suggestion`.
- **CLI** wraps the verb call in a try/catch; on `jt::Error`, it formats
  `Error at <path>: <problem>. <suggestion>` and `exit(1)`.

Same error text, two delivery paths. `jt::fail()` (the CLI-only exit helper)
is implemented as a thin catch → print → exit; it is **not** called from any
library function.

```cpp
// jt/error.hpp
class Error : public std::runtime_error {
 public:
  Error(std::string path, std::string problem, std::string suggestion = "")
      : std::runtime_error(problem), path_(std::move(path)),
        suggestion_(std::move(suggestion)) {}
  const std::string& path() const { return path_; }
  const std::string& suggestion() const { return suggestion_; }
 private:
  std::string path_, suggestion_;
};
```

### 8.3 Value semantics

- **Return-by-value with move** (`JsonDocument` is a `std::variant`, so moves
  are cheap; the document is consumed and returned, not mutated in place).
- **Literals** are `JsonDocument` values (parsed from JSON text at the call
  site), so `jt::set(doc, "/x", 42)` and `jt::set(doc, "/x", "hi")` are both
  natural via JSOM's implicit construction.
- **Comparison operators for `jtFilter`** are exposed as a small value type
  (`jt::op::eq / ne / gt / ge / lt / le`) so the library API is as typed as the
  CLI flags.

### 8.4 Consequences for the build

- `jt_core` is the installable library (header `jt/jt.hpp` aggregates the verb
  headers). JSOM is a transitive public dependency (its types appear in the
  API), so `jt_core` links `JSOM::jsom` with `PUBLIC`.
- CLI binaries remain separate executables; they now just wrap `jt_core`.

## 9. Resolved implementation notes (Aug 2026)

1. **`jtZip` duplicate keys** = error by default; `--overwrite` switches to
   last-wins on collision.
2. **`jtFilter` missing key** = element is dropped silently (fails the
   predicate).
3. **`jtLen` dest leaf** = **created** if absent (like `jtSet`'s leaf); only a
   missing *intermediate* errors unless `-p`.
