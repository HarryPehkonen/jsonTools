#include "jt/select.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"

namespace jt {

jsom::JsonDocument select(jsom::JsonDocument doc,
                          const std::vector<std::string>& paths) {
  if (paths.empty()) {
    throw Error("<args>", "no paths to select",
                "name at least one path, e.g. jtSelect /user/name");
  }

  jsom::JsonDocument picked = jsom::JsonDocument::make_object();
  for (const std::string& path : paths) {
    const std::string pointer = normalize_pointer(path);
    if (pointer.empty()) {
      throw Error("/", "the document root has no leaf key to select under",
                  "name a path inside it, e.g. /user/name");
    }

    const std::string leaf = jsom::JsonPointer::get_last_segment(pointer);
    if (picked.contains(leaf)) {
      throw Error(display_pointer(pointer), "duplicate leaf key '" + leaf + "'",
                  "two selected paths would flatten onto the same key");
    }
    picked.set(leaf, require_at(doc, pointer));
  }
  return picked;
}

} // namespace jt
