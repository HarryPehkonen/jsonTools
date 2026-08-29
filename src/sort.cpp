#include "jt/sort.hpp"

#include "jt/errors.hpp"
#include "jt/paths.hpp"

#include <algorithm>

namespace {

// Only scalars have a natural order; two objects being "less" than each other
// is a comparison jsonTools refuses to invent.
bool is_orderable(const jsom::JsonDocument& value) {
  return !value.is_object() && !value.is_array();
}

} // namespace

namespace jt {

namespace {

// Every value a sort compares must share one scalar type — no coercion, and no
// silent ordering across types. `where` names what is being compared.
void require_comparable(const jsom::JsonDocument& value,
                        const jsom::JsonDocument* first, const std::string& at,
                        const std::string& where) {
  if (!is_orderable(value)) {
    throw Error(at,
                "cannot sort " + where + ": " + type_name(value) +
                    " values have no natural order",
                "sort by a scalar key instead, e.g. jtSort /items name");
  }
  if (first != nullptr && first->type() != value.type()) {
    throw Error(at,
                "mixed types in " + where + ": " + type_name(*first) + " and " +
                    type_name(value),
                "jsonTools never coerces types; make the values uniform first");
  }
}

// The values a comparator will read, one per element, already type-checked.
std::vector<std::vector<const jsom::JsonDocument*>> collect_keys(
    const std::vector<jsom::JsonDocument>& elements,
    const std::vector<SortKey>& keys, const std::string& list_pointer) {
  std::vector<std::vector<const jsom::JsonDocument*>> columns;
  columns.reserve(keys.size());

  for (const SortKey& key : keys) {
    const std::string key_pointer = relative_key_pointer(key.key);
    std::vector<const jsom::JsonDocument*> column;
    column.reserve(elements.size());

    for (std::size_t i = 0; i < elements.size(); ++i) {
      // Built from the raw pointer, not display_pointer(): a root list is ""
      // here, so the element path reads "/0" rather than "//0".
      const std::string at = list_pointer + "/" + std::to_string(i);
      if (!elements[i].is_object()) {
        throw Error(at,
                    "a sort key needs object elements, got a " +
                        type_name(elements[i]),
                    "drop the key to sort the elements themselves");
      }
      const jsom::JsonDocument* value = elements[i].find(key_pointer);
      if (value == nullptr) {
        std::string hint = suggest_for(elements[i], key_pointer);
        if (hint.empty()) hint = "every element must carry the sort key";
        throw Error(at + key_pointer, "sort key not found", hint);
      }
      require_comparable(*value, column.empty() ? nullptr : column.front(),
                         at + key_pointer, "'" + key.key + "'");
      column.push_back(value);
    }
    columns.push_back(std::move(column));
  }
  return columns;
}

} // namespace

jsom::JsonDocument sort(jsom::JsonDocument doc, const std::string& list_path,
                        const std::vector<SortKey>& keys) {
  const std::string pointer = normalize_pointer(list_path);
  // Validate the keys before touching the document, exactly like filter():
  // an empty or malformed key is bad regardless of what the list turns out
  // to be (review issue 5).
  for (const SortKey& key : keys) {
    relative_key_pointer(key.key);
  }
  const jsom::JsonDocument& list = require_at(doc, pointer);
  if (!list.is_array()) {
    throw Error(display_pointer(pointer),
                "cannot sort a " + type_name(list) + "; jtSort needs an array",
                "point at the list, e.g. jtSort /items name");
  }

  std::vector<jsom::JsonDocument> elements = list.as_array();

  if (keys.empty()) {
    for (std::size_t i = 0; i < elements.size(); ++i) {
      require_comparable(elements[i], elements.empty() ? nullptr : &elements[0],
                         pointer + "/" + std::to_string(i), "the elements");
    }
    std::stable_sort(elements.begin(), elements.end(),
                     [](const jsom::JsonDocument& a, const jsom::JsonDocument& b) {
                       return a < b;
                     });
  } else {
    const auto columns = collect_keys(elements, keys, pointer);

    // Sort an index permutation so each element keeps its pre-checked values.
    std::vector<std::size_t> order(elements.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;

    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t lhs, std::size_t rhs) {
                       for (std::size_t k = 0; k < keys.size(); ++k) {
                         const jsom::JsonDocument& a = *columns[k][lhs];
                         const jsom::JsonDocument& b = *columns[k][rhs];
                         if (a < b) return !keys[k].descending;
                         if (b < a) return keys[k].descending;
                       }
                       return false;
                     });

    std::vector<jsom::JsonDocument> sorted;
    sorted.reserve(elements.size());
    for (std::size_t index : order) sorted.push_back(elements[index]);
    elements = std::move(sorted);
  }

  doc.set_at(pointer, jsom::JsonDocument(std::move(elements)));
  return doc;
}

} // namespace jt
