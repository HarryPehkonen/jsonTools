#include "jt/values.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"
#include "jt/set.hpp"

namespace jt {

jsom::JsonDocument values_array(jsom::JsonDocument doc,
                                const std::string& path) {
  const std::string pointer = normalize_pointer(path);
  const jsom::JsonDocument& target = require_at(doc, pointer);
  if (!target.is_object()) {
    throw Error(display_pointer(pointer),
                "a " + type_name(target) + " has no values",
                target.is_array() ? "an array's values are its elements"
                                  : "point at an object");
  }

  jsom::JsonDocument out = jsom::JsonDocument::make_array();
  for (const auto& entry : target.items()) {
    out.push_back(entry.second);
  }
  return out;
}

jsom::JsonDocument values(jsom::JsonDocument doc, const std::string& obj_path,
                          const std::string& dest_path, bool mkdir_p) {
  const std::string dest = normalize_pointer(dest_path);
  if (dest.empty()) {
    throw Error("/", "the values have nowhere to go at the document root",
                "name a field to write them to, e.g. jtValues /obj /values");
  }

  jsom::JsonDocument arr =
      values_array(jsom::JsonDocument(doc), normalize_pointer(obj_path));
  return set(std::move(doc), dest, arr, mkdir_p);
}

} // namespace jt
