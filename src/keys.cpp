#include "jt/keys.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"
#include "jt/set.hpp"

namespace jt {

jsom::JsonDocument keys(jsom::JsonDocument doc, const std::string& path) {
    const std::string pointer = normalize_pointer(path);
    const jsom::JsonDocument& target = require_at(doc, pointer);
    if (!target.is_object()) {
        throw Error(display_pointer(pointer), "a " + type_name(target) + " has no keys",
                    target.is_array() ? "use jtLen to count an array's elements"
                                      : "point at an object");
    }

    jsom::JsonDocument named = jsom::JsonDocument::make_array();
    for (const std::string& key : target.keys()) {
        named.push_back(jsom::JsonDocument(key));
    }
    return named;
}

jsom::JsonDocument keys_to(jsom::JsonDocument doc, const std::string& obj_path,
                           const std::string& dest_path, bool mkdir_p) {
    const std::string pointer = normalize_pointer(obj_path);
    const std::string dest = normalize_pointer(dest_path);
    if (dest.empty()) {
        throw Error("/", "the keys have nowhere to go at the document root",
                    "name a field to write them to, e.g. jtKeys /obj /keys");
    }

    jsom::JsonDocument named = keys(jsom::JsonDocument(doc), pointer);
    return set(std::move(doc), dest, named, mkdir_p);
}

} // namespace jt
