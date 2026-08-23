#include "jt/filter.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"

namespace jt {

namespace {

bool is_ordering(Op op) { return op != Op::Eq && op != Op::Ne; }

bool apply(Op op, const jsom::JsonDocument& lhs, const jsom::JsonDocument& rhs) {
  switch (op) {
    case Op::Eq: return lhs == rhs;
    case Op::Ne: return !(lhs == rhs);
    case Op::Gt: return rhs < lhs;
    case Op::Ge: return !(lhs < rhs);
    case Op::Lt: return lhs < rhs;
    case Op::Le: return !(rhs < lhs);
  }
  return false;
}

} // namespace

bool op_from_flag(const std::string& flag, Op& out) {
  if (flag == "--eq") { out = Op::Eq; return true; }
  if (flag == "--ne") { out = Op::Ne; return true; }
  if (flag == "--gt") { out = Op::Gt; return true; }
  if (flag == "--ge") { out = Op::Ge; return true; }
  if (flag == "--lt") { out = Op::Lt; return true; }
  if (flag == "--le") { out = Op::Le; return true; }
  return false;
}

jsom::JsonDocument filter(jsom::JsonDocument doc, const std::string& list_path,
                          const std::string& key_path, Op op,
                          const jsom::JsonDocument& value) {
  const std::string pointer = normalize_pointer(list_path);
  const std::string key_pointer = relative_key_pointer(key_path);

  const jsom::JsonDocument& list = require_at(doc, pointer);
  if (!list.is_array()) {
    throw Error(display_pointer(pointer),
                "cannot filter a " + type_name(list) + "; jtFilter needs an array",
                "point at the list, e.g. jtFilter /items age --gt 18");
  }

  if (is_ordering(op) && !value.is_number() && !value.is_string()) {
    throw Error("<literal>",
                "cannot order-compare against a " + type_name(value),
                "--gt/--ge/--lt/--le take a number or a string");
  }

  const std::vector<jsom::JsonDocument>& elements = list.as_array();
  std::vector<jsom::JsonDocument> kept;
  for (std::size_t i = 0; i < elements.size(); ++i) {
    // A missing key simply fails the predicate — the one silent drop jtFilter
    // makes (TECHNICAL_DETAILS §9).
    const jsom::JsonDocument* found =
        elements[i].is_object() ? elements[i].find(key_pointer) : nullptr;
    if (found == nullptr) continue;

    if (is_ordering(op) && found->type() != value.type()) {
      throw Error(pointer + "/" + std::to_string(i) + key_pointer,
                  "cannot order a " + type_name(*found) + " against a " +
                      type_name(value),
                  "jsonTools never coerces types; compare like with like");
    }
    if (apply(op, *found, value)) kept.push_back(elements[i]);
  }

  doc.set_at(pointer, jsom::JsonDocument(std::move(kept)));
  return doc;
}

} // namespace jt
