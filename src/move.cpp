#include "jt/move.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"
#include "jt/set.hpp"

namespace jt {

jsom::JsonDocument move(jsom::JsonDocument doc, const std::string& from, const std::string& to,
                        DestMode mode) {
    const std::string src = normalize_pointer(from);
    const std::string dst = normalize_pointer(to);

    if (src.empty()) {
        throw Error("/", "cannot move the document root", "there is nothing above it to move into");
    }
    if (src != dst && jsom::JsonPointer::is_prefix(src, dst)) {
        throw Error(display_pointer(dst), "cannot move a value into its own child",
                    "pick a destination outside " + src);
    }

    jsom::JsonDocument value = require_at(doc, src);
    if (!dest_write_allowed(doc, dst, mode))
        return doc;

    // Check the destination before destroying the source, so a move that cannot
    // land never half-completes.
    require_parent(doc, dst, "create the missing objects first");

    doc.remove_at(src);
    return set(std::move(doc), dst, value);
}

} // namespace jt
