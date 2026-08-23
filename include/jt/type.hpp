#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// The document becomes the JSON type name at `path` ("object", "array",
// "string", "number", "boolean", "null"). A missing path is an error.
jsom::JsonDocument type(jsom::JsonDocument doc, const std::string& path = "/");

} // namespace jt
