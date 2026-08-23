#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// The document becomes `{ keys[0]: values[0], ... }`. Both paths must resolve
// to arrays of the same length, and the keys must be strings. Colliding keys
// are an error unless `overwrite`, which makes the last one win.
jsom::JsonDocument zip(jsom::JsonDocument doc, const std::string& keys_path,
                       const std::string& values_path, bool overwrite = false);

} // namespace jt
