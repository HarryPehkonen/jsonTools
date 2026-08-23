#pragma once

#include <jsom/jsom.hpp>

#include <string>
#include <vector>

namespace jt {

// Build a fresh object from `paths`, each value stored under its **leaf key**
// ("/user/name" -> "name"). Zero paths, a duplicate leaf key, a missing path
// and the root pointer (which has no leaf key) are all errors.
jsom::JsonDocument select(jsom::JsonDocument doc,
                          const std::vector<std::string>& paths);

} // namespace jt
