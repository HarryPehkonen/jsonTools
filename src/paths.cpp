#include "jt/paths.hpp"

#include "jt/errors.hpp"

namespace jt {

std::string display_pointer(const std::string& pointer) {
  return pointer.empty() ? "/" : pointer;
}

std::string normalize_pointer(const std::string& path) {
  if (path.empty() || path == "/") return "";
  if (path[0] != '/') {
    throw Error(path, "path must be an absolute JSON Pointer",
                "write it with a leading slash, e.g. /" + path);
  }
  try {
    jsom::JsonPointer::validate(path);
  } catch (const jsom::JsonPointerException& e) {
    throw Error(path, "malformed JSON Pointer", e.what());
  }
  return path;
}

std::string type_name(const jsom::JsonDocument& doc) {
  if (doc.is_null()) return "null";
  if (doc.is_bool()) return "boolean";
  if (doc.is_number()) return "number";
  if (doc.is_string()) return "string";
  if (doc.is_object()) return "object";
  return "array";
}

std::string suggest_for(const jsom::JsonDocument& doc, const std::string& pointer) {
  std::vector<std::string> segments;
  try {
    segments = jsom::JsonPointer::parse(pointer);
  } catch (const jsom::JsonPointerException&) {
    return "";
  }

  std::string prefix;
  const jsom::JsonDocument* current = &doc;
  for (const std::string& segment : segments) {
    std::string child = prefix + "/" + jsom::JsonPointer::escape_segment(segment);
    const jsom::JsonDocument* next = doc.find(child);
    if (next == nullptr) {
      if (current->is_object()) {
        std::string best = nearest_candidate(current->keys(), segment);
        if (!best.empty()) {
          return "did you mean " + prefix + "/" +
                 jsom::JsonPointer::escape_segment(best) + "?";
        }
      }
      return "";
    }
    current = next;
    prefix = child;
  }
  return "";
}

// The bounds half of every out-of-range array hint (FIX_ME A.1): require_at,
// require_parent and create_object_path all describe the offending array the
// same way, so the wording lives here once and each call site appends only
// its own remediation. `with_indexes` drops the "(indexes 0-M)" clause for
// require_parent's one-past-the-end case, where the append advice replaces
// the range.
static std::string array_bounds_hint(const std::string& pointer,
                                     std::size_t size, bool with_indexes) {
  std::string hint = "the array at " + display_pointer(pointer);
  if (size == 0) return hint + " is empty";
  hint += " has " + std::to_string(size) + " elements";
  if (with_indexes) hint += " (indexes 0-" + std::to_string(size - 1) + ")";
  return hint;
}

const jsom::JsonDocument& require_at(const jsom::JsonDocument& doc,
                                     const std::string& pointer) {
  // find()==nullptr alone cannot tell *why* a path failed, so walk the
  // segments and classify at the first one that does not resolve (review
  // issue 3): a missing object key, an array addressing error, or a path
  // that tunnels through a scalar. Each gets its own message, reported at
  // the failing segment.
  std::vector<std::string> segments;
  try {
    segments = jsom::JsonPointer::parse(pointer);
  } catch (const jsom::JsonPointerException&) {
    // Callers run normalize_pointer first; a malformed pointer here can
    // only fall back to the generic message.
    throw Error(display_pointer(pointer), "path not found", "");
  }

  std::string prefix;
  const jsom::JsonDocument* current = &doc;
  for (const std::string& segment : segments) {
    const std::string child =
        prefix + "/" + jsom::JsonPointer::escape_segment(segment);
    if (current->is_object()) {
      if (!current->contains(segment)) {
        // A genuinely absent key — the one case that is truly "not found";
        // it keeps the typo hint.
        throw Error(display_pointer(child), "path not found",
                    suggest_for(doc, pointer));
      }
      current = &(*current)[segment];
    } else if (current->is_array()) {
      if (jsom::JsonPointer::is_append(segment)) {
        throw Error(display_pointer(child), "'-' names no existing element",
                    "the '-' sentinel only appends on write; read by index, e.g. " +
                        display_pointer(prefix) + "/0");
      }
      if (!jsom::JsonPointer::is_array_index(segment)) {
        throw Error(display_pointer(child),
                    "'" + segment + "' is not an array index",
                    "array elements are addressed by index, e.g. " +
                        display_pointer(prefix) + "/0");
      }
      // to_array_index overflows to an exception for indexes beyond size_t;
      // such an index is certainly out of range, so keep the default.
      std::size_t index = current->size();
      try {
        index = jsom::JsonPointer::to_array_index(segment);
      } catch (const jsom::JsonPointerException&) {
      }
      const std::size_t size = current->size();
      if (index >= size) {
        throw Error(display_pointer(child),
                    "index " + segment + " is out of range",
                    array_bounds_hint(prefix, size, true));
      }
      current = &(*current)[index];
    } else {
      // The path tunnels through a scalar, which holds no keys at all.
      throw Error(display_pointer(child),
                  "cannot look up '" + segment + "' inside a " +
                      type_name(*current),
                  "use " + display_pointer(prefix) + " to address the " +
                      type_name(*current) + " itself");
    }
    prefix = child;
  }
  return *current;
}

void require_parent(const jsom::JsonDocument& doc, const std::string& pointer,
                    const std::string& fallback_hint) {
  if (pointer.empty()) return;
  const std::string parent = jsom::JsonPointer::get_parent(pointer);
  const jsom::JsonDocument* container = doc.find(parent);
  if (container == nullptr) {
    std::string hint = suggest_for(doc, parent);
    if (hint.empty()) hint = fallback_hint;
    throw Error(display_pointer(parent), "missing intermediate path", hint);
  }
  if (container->is_object()) return; // any leaf key can live in an object

  if (container->is_array()) {
    const std::string leaf = jsom::JsonPointer::get_last_segment(pointer);
    if (jsom::JsonPointer::is_append(leaf)) return; // "-" grows the array by one
    if (!jsom::JsonPointer::is_array_index(leaf)) {
      throw Error(display_pointer(pointer),
                  "'" + leaf + "' is not an array index",
                  "array elements are addressed by index, e.g. " +
                      display_pointer(parent) + "/0");
    }
    // to_array_index overflows to an exception for indexes beyond size_t;
    // such an index is certainly out of range, so keep the default.
    std::size_t index = container->size();
    try {
      index = jsom::JsonPointer::to_array_index(leaf);
    } catch (const jsom::JsonPointerException&) {
    }
    const std::size_t size = container->size();
    if (index < size) return;
    std::string hint;
    if (size > 0 && index == size) {
      // One past the end: the append advice replaces the index range.
      hint = array_bounds_hint(parent, size, false) +
             "; append with the '-' sentinel instead";
    } else {
      hint = array_bounds_hint(parent, size, true);
    }
    throw Error(display_pointer(pointer),
                "index " + leaf + " is out of range", hint);
  }

  throw Error(display_pointer(parent),
              "cannot put a value inside a " + type_name(*container),
              "remove or replace that value first");
}

void create_object_path(jsom::JsonDocument& doc, const std::string& pointer) {
  std::vector<std::string> segments = jsom::JsonPointer::parse(pointer);
  std::string prefix;
  jsom::JsonDocument* current = &doc;
  // Walk the intermediate segments only — the leaf is left to the caller's
  // write (set_at), so '-' as the final segment never passes through here.
  for (std::size_t i = 0; i + 1 < segments.size(); ++i) {
    const std::string& segment = segments[i];
    const std::string here = prefix + "/" + jsom::JsonPointer::escape_segment(segment);
    if (current->is_array()) {
      if (jsom::JsonPointer::is_append(segment)) {
        throw Error(display_pointer(here), "'-' is only valid as the final segment",
                    "append an element first, e.g. jtSet " + display_pointer(prefix) +
                        "/- '{}'; it becomes " + display_pointer(prefix) + "/" +
                        std::to_string(current->size()) + ", then re-run this command");
      }
      if (!jsom::JsonPointer::is_array_index(segment)) {
        throw Error(display_pointer(here),
                    "'" + segment + "' is not an array index",
                    "array elements are addressed by index, e.g. " +
                        display_pointer(prefix) + "/0");
      }
      // to_array_index overflows to an exception for indexes beyond size_t;
      // such an index is certainly out of range, so keep the default.
      std::size_t index = current->size();
      try {
        index = jsom::JsonPointer::to_array_index(segment);
      } catch (const jsom::JsonPointerException&) {
      }
      const std::size_t size = current->size();
      if (index >= size) {
        std::string hint;
        if (index == size) {
          hint = "append an element first, e.g. jtSet " + display_pointer(prefix) +
                 "/- '{}'; it becomes " + display_pointer(prefix) + "/" +
                 std::to_string(size) + ", then re-run this command";
        } else {
          hint = array_bounds_hint(prefix, size, true) +
                 "; arrays grow one element at a time with the '-' sentinel";
        }
        throw Error(display_pointer(here),
                    "index " + segment + " is out of range", hint);
      }
      current = &(*current)[index];
    } else if (current->is_object()) {
      if (!current->contains(segment)) {
        current->set(segment, jsom::JsonDocument::make_object());
      }
      current = &(*current)[segment];
    } else {
      throw Error(display_pointer(prefix),
                  "cannot create '" + segment + "' inside a " + type_name(*current),
                  "remove or replace that value first");
    }
    prefix = here;
  }
}

std::string relative_key_pointer(const std::string& key_path) {
  if (key_path.empty()) {
    throw Error("<key>", "empty key path",
                "give a key relative to each element, e.g. name or name/last");
  }
  if (key_path[0] == '/') {
    throw Error(key_path, "key paths are relative to each element",
                "drop the leading slash: " + key_path.substr(1));
  }
  std::string pointer = "/" + key_path;
  try {
    jsom::JsonPointer::validate(pointer);
  } catch (const jsom::JsonPointerException& e) {
    throw Error(key_path, "malformed key path", e.what());
  }
  return pointer;
}

} // namespace jt
