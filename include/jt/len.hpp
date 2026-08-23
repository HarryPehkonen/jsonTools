#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// Write the length of the array or object at `list_path` to `dest_path`,
// leaving the rest of the document intact. The destination leaf is created; a
// missing *intermediate* is an error unless `mkdir_p`.
jsom::JsonDocument len(jsom::JsonDocument doc, const std::string& list_path,
                       const std::string& dest_path, bool mkdir_p = false);

} // namespace jt
