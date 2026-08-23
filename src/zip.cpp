#include "jt/zip.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"

namespace jt {

namespace {

const jsom::JsonDocument& require_array(const jsom::JsonDocument& doc,
                                        const std::string& pointer,
                                        const char* role) {
  const jsom::JsonDocument& value = require_at(doc, pointer);
  if (!value.is_array()) {
    throw Error(display_pointer(pointer),
                std::string("the ") + role + " path is a " + type_name(value) +
                    ", not an array",
                "jtZip pairs two arrays element by element");
  }
  return value;
}

} // namespace

jsom::JsonDocument zip(jsom::JsonDocument doc, const std::string& keys_path,
                       const std::string& values_path, bool overwrite) {
  const std::string keys_pointer = normalize_pointer(keys_path);
  const std::string values_pointer = normalize_pointer(values_path);

  const std::vector<jsom::JsonDocument>& keys =
      require_array(doc, keys_pointer, "keys").as_array();
  const std::vector<jsom::JsonDocument>& values =
      require_array(doc, values_pointer, "values").as_array();

  if (keys.size() != values.size()) {
    throw Error(display_pointer(keys_pointer),
                "the key and value lists differ in length (" +
                    std::to_string(keys.size()) + " vs " +
                    std::to_string(values.size()) + ")",
                "jtZip needs one value per key");
  }

  jsom::JsonDocument zipped = jsom::JsonDocument::make_object();
  for (std::size_t i = 0; i < keys.size(); ++i) {
    if (!keys[i].is_string()) {
      throw Error(keys_pointer + "/" + std::to_string(i),
                  "object keys must be strings, got a " + type_name(keys[i]),
                  "quote the key in the source list");
    }
    std::string key = keys[i].as<std::string>();
    if (!overwrite && zipped.contains(key)) {
      throw Error(keys_pointer + "/" + std::to_string(i),
                  "duplicate key '" + key + "'",
                  "pass --overwrite to let the last value win");
    }
    zipped.set(key, values[i]);
  }
  return zipped;
}

} // namespace jt
