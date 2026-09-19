#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// The document becomes the JSON type name at `path` ("object", "array",
// "string", "number", "boolean", "null"). A missing path is an error. Reads
// `doc` only, so it takes it by const reference.
jsom::JsonDocument type(const jsom::JsonDocument& doc, const std::string& path = "/");

} // namespace jt
