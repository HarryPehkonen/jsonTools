#include "jt/set.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"

namespace jt {

jsom::JsonDocument set(jsom::JsonDocument doc, const std::string& path,
                       const jsom::JsonDocument& literal, bool mkdir_p) {
  const std::string pointer = normalize_pointer(path);
  if (pointer.empty()) return literal; // setting the root replaces the document

  if (mkdir_p) {
    create_object_path(doc, pointer);
    // The parents exist now; this validates the leaf's own container (an
    // array's index range or a scalar in the way).
    require_parent(doc, pointer, "");
  } else {
    require_parent(doc, pointer, "pass -p to create the missing objects");
  }

  try {
    doc.set_at(pointer, literal);
  } catch (const jsom::JsonPointerException& e) {
    throw Error(display_pointer(pointer), "cannot set a value here", e.what());
  }
  return doc;
}

} // namespace jt
