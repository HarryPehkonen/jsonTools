#include "jt/len.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"
#include "jt/set.hpp"

namespace jt {

jsom::JsonDocument len(jsom::JsonDocument doc, const std::string& list_path,
                       const std::string& dest_path, bool mkdir_p) {
    const std::string pointer = normalize_pointer(list_path);
    const std::string dest = normalize_pointer(dest_path);
    if (dest.empty()) {
        throw Error("/", "the length has nowhere to go at the document root",
                    "name a field to write it to, e.g. jtLen /items /count");
    }

    const jsom::JsonDocument& list = require_at(doc, pointer);
    if (!list.is_array() && !list.is_object()) {
        throw Error(display_pointer(pointer), "a " + type_name(list) + " has no length",
                    "jtLen counts an array's elements or an object's keys");
    }

    const auto count = static_cast<long long>(list.size());
    return set(std::move(doc), dest, jsom::JsonDocument::from_lazy_number(std::to_string(count)),
               mkdir_p);
}

} // namespace jt
