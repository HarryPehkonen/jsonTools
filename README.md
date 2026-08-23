# jsonTools

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

This installs the 14 CLI binaries, the static `libjt_core.a`, and the `jt/`
headers (for use as a library).

## The verbs

```
jtNew      jtFrom      jtSet      jtMove     jtCopy
jtRemove   jtGet       jtSelect   jtZip      jtSort
jtFilter   jtLen       jtType     jtKeys
```

All paths are strict [RFC 6901 JSON Pointers](https://datatracker.ietf.org/doc/html/rfc6901).
Per-element verbs (`jtSort`, `jtFilter`) take element-relative keys (no leading
slash); document verbs take absolute pointers (leading slash).

See `REQUIREMENTS.md` for full semantics, and `TECHNICAL_DETAILS.md` for the
implementation mapping and the C++ library API.

## Examples

```bash
# Build an object from nothing
jtNew '{}' | jtSet /name '"harri"' | jtSet /age 48

# Reshape
echo '{"user":{"name":"harri","age":48}}' | jtMove /user/name /name

# Narrow + sort + project
echo '{"users":[{"name":"ada","age":36},{"name":"harri","age":48}]}' \
  | jtFilter /users age --gt 40 \
  | jtSort /users name \
  | jtSelect /users

# Append to an array (RFC 6902 "-" sentinel)
echo '{"items":["a","b"]}' | jtSet /items/- '"c"'
```

## Errors

Errors are `Error at <path>: <problem>. <suggestion>` and exit non-zero —
including Levenshtein "did you mean" hints for typos. jsonTools never silently
coerces types or hides a missing path.
