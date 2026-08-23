#pragma once

#include <jsom/jsom.hpp>

#include <string>

namespace jt {

// Reduction form: the document becomes the array of key names of the object
// at `path`. A missing path, or anything that is not an object, is an error.
jsom::JsonDocument keys(jsom::JsonDocument doc, const std::string& path = "/");

// Form B (composable): write the array of the object's KEYS at `obj_path` into
// `dest_path`, leaving the document intact. The destination leaf is created; a
// missing intermediate is an error unless `mkdir_p`.
jsom::JsonDocument keys_to(jsom::JsonDocument doc, const std::string& obj_path,
                           const std::string& dest_path, bool mkdir_p = false);

} // namespace jt
