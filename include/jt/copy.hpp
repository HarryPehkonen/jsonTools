#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// What jtMove/jtCopy are allowed to do at the destination.
enum class DestMode {
    Overwrite, // default: always write
    IfNotSet,  // --if-not-set: write only when <to> is absent
    Replace,   // --replace: write only when <to> already exists
};

// True when `mode` permits writing to `to`. Shared with jtMove. The append
// sentinel never "exists", so `--if-not-set /list/-` appends while
// `--replace /list/-` does nothing.
bool dest_write_allowed(const jsom::JsonDocument& doc, const std::string& to, DestMode mode);

// Copy the value at `from` to `to`, keeping the source. When the destination
// guard blocks the write the document comes back unchanged — that is a
// condition, not an error. Throws jt::Error when `from` is missing or the
// destination's parent does not exist.
jsom::JsonDocument copy(jsom::JsonDocument doc, const std::string& from, const std::string& to,
                        DestMode mode = DestMode::Overwrite);

} // namespace jt
