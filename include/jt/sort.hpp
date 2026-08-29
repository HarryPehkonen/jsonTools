#pragma once

#include <jsom/jsom.hpp>

#include <string>
#include <vector>

namespace jt {

// One sort key: an element-relative path ("name", "name/last") and its
// direction. `--desc-for <key>` on the CLI sets `descending` for that key only.
struct SortKey {
  std::string key;
  bool descending = false;
};

// Sort the list at `list_path` (default "/", the whole document — bare
// `jtSort` natural-sorts the document itself) in place, stably. With keys,
// every element must be an object carrying every key, and a key's values must
// all share one scalar type. Keys are validated (non-empty, no leading
// slash) before the list is resolved, like filter(). Without keys, the
// elements themselves are sorted by their natural type — mixed types are an
// error.
jsom::JsonDocument sort(jsom::JsonDocument doc, const std::string& list_path,
                        const std::vector<SortKey>& keys);

} // namespace jt
