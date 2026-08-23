#include "jt/type.hpp"

#include "jt/paths.hpp"

namespace jt {

jsom::JsonDocument type(jsom::JsonDocument doc, const std::string& path) {
  const std::string pointer = normalize_pointer(path);
  return jsom::JsonDocument(type_name(require_at(doc, pointer)));
}

} // namespace jt
