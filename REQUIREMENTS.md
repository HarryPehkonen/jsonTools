# jsonTools — Requirements & Design

> Draft for review. Every "**OPEN**" marker is a decision not yet final; every
> other statement is a settled requirement from design discussion (Aug 2026).

## 1. Purpose

jsonTools is a suite of small, composable command-line filters for JSON. It
re-skins the transformation power of [Computo](https://github.com/HarryPehkonen/Computo)
into a friendlier, *debuggable* form, borrowing the philosophy of the original
[tabTools](https://github.com/HarryPehkonen/tabTools) suite.

The one-line goal: **make JSON transforms composable one mutation at a time,
so every intermediate state is visible and every step is independently
inspectable.**

Computo's pain point is that a whole transformation is one expression tree,
evaluated in one shot — hard to debug because you cannot see *where* it went
wrong. jsonTools fixes this by putting each mutation in its own process,
joined by pipes.

## 2. Design principles

1. **One mutation per tool.** Each tool does exactly one thing to a JSON
   document. Composition happens with pipes, not with a script.
2. **Stay close to the data.** Every tool reads JSON on stdin and writes JSON
   on stdout, so `... | jtGet /a | jtGet /b` shows you each step.
3. **Arbitrary JSON.** Not just tables — any object or array, any nesting.
4. **Safety.** No shell execution, no code evaluation, no shared mutable files,
   no "surprising places" where values can come from. Strict errors over
   silent `null`s.
5. **Plain JSON on the wire.** Tools never emit a private envelope; output is
   always valid JSON a human (or `jq`) can read.

## 3. Foundation

jsonTools is a **thin mutation layer over
[JSOM](https://github.com/HarryPehkonen/JSOM)**, not over libcomputo.

JSOM already provides what jsonTools needs and nothing more:
- RFC 6901 JSON Pointer resolution with path caching
- Intelligent formatting presets (compact / pretty / config / API / debug)
- Zero-dependency C++17, `std::variant`, RAII
- (Streaming parser — noted but not required; see §7)

jsonTools adds: the `jt*` verbs, the mutation semantics below, the CLI
arg parsing, and the error model. It does **not** link libcomputo's
expression-tree evaluator, which is a different abstraction (evaluator vs
mutator).

## 4. Global conventions

- **Language**: C++17 (or newer). **Build**: CMake.
- **Binaries**: each tool is a separate executable (`jtSet`, `jtMove`, …),
  one `.cpp` source file each; shared helpers linked from a common library.
  A single top-level `CMakeLists.txt` builds all binaries (tests may use
  their own).
- **Paths**: JSON Pointer (RFC 6901) everywhere — `/user/name`, `/items/0`,
  `/`. Relative sub-paths (inside a list element) have no leading slash.
- **I/O**: stdin → one JSON document in; stdout → one JSON document out.
- **Optional `[listPath]`**: any verb acting on a container takes an optional
  first path defaulting to `/`. When the flowing document *is* the list, the
  path is omitted.
- **Pretty-print**: opt-in, from the shared JSOM formatting code
  (`--pretty`, or a preset flag). Default output is compact.
- **Memory model**: read the whole document → mutate → emit → free. Each
  process frees everything on exit, so a long pipeline never accumulates.
  jsonTools is **not** streaming (a tree mutation has no streaming unit);
  RAM-exceeding inputs are out of scope.

## 5. Verbs

### Construction
| Verb | Semantics |
|---|---|
| `jtNew` | Emit a blank document (`{}` or `[]`, or a given literal). Start of a build-from-nothing pipe. |
| `jtFrom` | Read a file as the flowing document; validate it (syntax, depth, optional size/dup-key checks) before emitting. |

### Core mutation
| Verb | Semantics |
|---|---|
| `jtSet` | Set `/path` to a **literal** value (JSON text). `mkdir -p` style: **error if an intermediate path segment is missing unless `-p`** creates it. |
| `jtMove` | Move the value at `/from` to `/to` (rename = move within the same parent). Destination behavior via flags: **default = always overwrite**; `--if-not-set` = only when `/to` is absent; `--replace` = only when `/to` already exists (overwrite existing, never create new). |
| `jtCopy` | Copy the value at `/from` to `/to` (source retained). |
| `jtRemove` | Delete `/path`. |

### Extract / read
| Verb | Semantics |
|---|---|
| `jtGet` | Emit the value at `/path` (document becomes just it). `/missing` is an **error** unless `--default '"value"'` (JSON-literal syntax). |
| `jtSelect` | Pick multiple paths into a fresh object, **flattening to leaf keys** (`{"name":…, "age":…}`); duplicate leaf keys are an **error**. |

### Shape
| Verb | Semantics |
|---|---|
| `jtZip` | Build an object from two paths in the same document (keys list + values list). **Bail with an error if the two lists have different lengths**, or if two keys collide — unless `--overwrite` (last-wins on collision). (Future: stop / default-value modes for length mismatch.) |
| `jtSort` | Sort a list (`jtSort <listPath> <key1> --desc-for <key2> <key3>`). Multiple keys, mixed directions. No keys → sort by element natural type (numbers numerically, strings lexically); **mixed types = error**. "Typical JavaScript" coercion is a **future switchable option**. |
| `jtFilter` | Filter a list: `jtFilter [listPath] <keyPath> --op <literal>`. `keyPath` is relative to each element. Operators: `--eq --ne --gt --ge --lt --le` (and string equality for `--eq`). Value is a JSON literal so type is unambiguous. |
| `jtLen` | **Form B**: read the length of the list at `listPath`, write it to `destPath`, leave the document intact. (`jtLen /items /count` → `{"items":[…], "count":7}`.) |

### Deferred (not in v1)
- **`jtRun`** — batch script = serialized pipeline (one `jt*` command per line).
- **`jtMap`** — future shape `jtMap '/list' transform.jt` = run a batch script per element (requires `jtRun` first; this is why the interpreted-language question dissolved).
- **`jtFreq`** — frequency count (`value → count`), the tabTools `ttCount` behavior that `jtLen` (length) is distinct from.
- **`jtMerge`** — merge a second object (future: from a file arg).

## 6. Error model

Reuse Computo's style:

```
Error at <path>: <problem>. <suggestion>
```

- Paths are JSON Pointers so they can be pasted straight into a tool.
- `<suggestion>` includes a Levenshtein "did you mean `/usr`?" typo hint where
  the failing path resembles a real key.
- Exit code non-zero on any error; the document is not emitted on failure.

## 7. Safety rules

- Values come from **literals** (explicit JSON) or **explicit paths in the
  current document** — never from shell, environment, or evaluation of
  arbitrary code.
- No temp files, no shared mutable state between tools (pipes only).
- Strict typing: no silent numeric/string coercion (`"1"` is not `1`).
- `jtFrom` validates before emitting (syntax, configurable depth/size caps,
  optional duplicate-key warning).

## 8. Example pipelines

```bash
# Reshape: pick fields, flatten
cat user.json | jtSelect /user/name /user/age

# Build from nothing
jtNew '{}' | jtSet /name '"harri"' | jtSet /tags '["a","b"]'

# Filter + sort
cat data.json | jtFilter '/items' 'age' --gt 18 | jtSort '/items' 'name'

# Mutate: rename + delete
cat doc.json | jtMove /oldKey /newKey | jtRemove /junk
```

## 9. Resolved decisions (Aug 2026)

These were open during design and are now settled:

1. **`jtMove --replace`** = overwrite only when `/to` already exists (never
   create new); `--if-not-set` = only when absent; default = always overwrite.
2. **`jtSort` no-keys** = sort by element natural type (numbers numerically,
   strings lexically, mixed types = error). JS-coercion stays a future option.
3. **`jtGet --default`** = JSON-literal syntax (`--default '"value"'`),
   consistent with `jtSet` and `jtFilter`.
4. **`jtSelect` duplicate leaf keys** = error (not silent last-wins).

## 10. Non-goals (explicit)

- Not a database, not a query language, not a REPL (that's Computo's job).
- No streaming / RAM-exceeding input support.
- No schema validation or typing beyond JSON's own types.
