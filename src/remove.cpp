#include "jt/remove.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"

namespace jt {

jsom::JsonDocument remove(jsom::JsonDocument doc, const std::string& path) {
  const std::string pointer = normalize_pointer(path);
  if (pointer.empty()) {
    throw Error("/", "cannot remove the document root",
                "pipe through jtNew if you want to start over");
  }

  require_at(doc, pointer); // missing -> "path not found" with a typo hint
  if (!doc.remove_at(pointer)) {
    throw Error(display_pointer(pointer), "cannot remove a value here",
                "its parent is not an object or array");
  }
  return doc;
}

} // namespace jt
