#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// Set `literal` at `path`. The leaf is created; a missing *intermediate* is an
// error unless `mkdir_p`, which creates objects only and descends into
// existing array elements by index — but never creates or grows an array. The
// RFC 6902 append sentinel ("/items/-") appends to an existing array.
jsom::JsonDocument set(jsom::JsonDocument doc, const std::string& path,
                       const jsom::JsonDocument& literal, bool mkdir_p = false);

} // namespace jt
