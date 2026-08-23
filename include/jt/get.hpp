#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// The document becomes the value at `path`. "/" is the identity (allowed, for
// debugging). A missing path yields `fallback` when one is given, and is an
// error otherwise.
jsom::JsonDocument get(jsom::JsonDocument doc, const std::string& path,
                       const jsom::JsonDocument* fallback = nullptr);

} // namespace jt
