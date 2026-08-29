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
