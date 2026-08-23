#include "jt/copy.hpp"

#include "jt/paths.hpp"
#include "jt/set.hpp"

namespace jt {

bool dest_write_allowed(const jsom::JsonDocument& doc, const std::string& to,
                        DestMode mode) {
  switch (mode) {
    case DestMode::IfNotSet:
      return doc.find(to) == nullptr;
    case DestMode::Replace:
      return doc.find(to) != nullptr;
    case DestMode::Overwrite:
      break;
  }
  return true;
}

jsom::JsonDocument copy(jsom::JsonDocument doc, const std::string& from,
                        const std::string& to, DestMode mode) {
  const std::string src = normalize_pointer(from);
  const std::string dst = normalize_pointer(to);

  jsom::JsonDocument value = require_at(doc, src); // copied out before any write
  if (!dest_write_allowed(doc, dst, mode)) return doc;
  return set(std::move(doc), dst, value);
}

} // namespace jt
