#include "jt/get.hpp"

#include "jt/paths.hpp"

namespace jt {

jsom::JsonDocument get(jsom::JsonDocument doc, const std::string& path,
                       const jsom::JsonDocument* fallback) {
    const std::string pointer = normalize_pointer(path);
    if (pointer.empty())
        return doc; // jtGet / is the identity

    const jsom::JsonDocument* found = doc.find(pointer);
    if (found == nullptr) {
        if (fallback != nullptr)
            return *fallback;
        return require_at(doc, pointer); // throws, with a typo hint
    }
    return *found;
}

} // namespace jt
