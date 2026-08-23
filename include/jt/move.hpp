#pragma once

#include "jt/copy.hpp" // DestMode

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// Move the value at `from` to `to`. The source is removed only when the write
// actually happens, so a guard that blocks the write leaves the document
// untouched. Moving the document root, or into a child of `from`, is an error.
jsom::JsonDocument move(jsom::JsonDocument doc, const std::string& from,
                        const std::string& to, DestMode mode = DestMode::Overwrite);

} // namespace jt
