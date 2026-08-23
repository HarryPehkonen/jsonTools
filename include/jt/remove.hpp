#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// Delete the value at `path`. A missing path is an error (with a nearest-key
// hint); the document root cannot be removed.
jsom::JsonDocument remove(jsom::JsonDocument doc, const std::string& path);

} // namespace jt
