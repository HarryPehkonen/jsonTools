#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// The document becomes the array of key names of the object at `path`. A
// missing path, or anything that is not an object, is an error.
jsom::JsonDocument keys(jsom::JsonDocument doc, const std::string& path = "/");

} // namespace jt
