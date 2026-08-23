#include "jt/keys.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"

namespace jt {

jsom::JsonDocument keys(jsom::JsonDocument doc, const std::string& path) {
  const std::string pointer = normalize_pointer(path);
  const jsom::JsonDocument& target = require_at(doc, pointer);
  if (!target.is_object()) {
    throw Error(display_pointer(pointer),
                "a " + type_name(target) + " has no keys",
                target.is_array() ? "use jtLen to count an array's elements"
                                  : "point at an object");
  }

  jsom::JsonDocument named = jsom::JsonDocument::make_array();
  for (const std::string& key : target.keys()) {
    named.push_back(jsom::JsonDocument(key));
  }
  return named;
}

} // namespace jt
