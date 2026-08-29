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

const jsom::JsonDocument& require_at(const jsom::JsonDocument& doc,
                                     const std::string& pointer) {
  const jsom::JsonDocument* found = doc.find(pointer);
  if (found == nullptr) {
    throw Error(display_pointer(pointer), "path not found", suggest_for(doc, pointer));
  }
  return *found;
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
    if (size == 0) {
      hint = "the array at " + display_pointer(parent) + " is empty";
    } else if (index == size) {
      hint = "the array at " + display_pointer(parent) + " has " +
             std::to_string(size) +
             " elements; append with the '-' sentinel instead";
    } else {
      hint = "the array at " + display_pointer(parent) + " has " +
             std::to_string(size) + " elements (indexes 0-" +
             std::to_string(size - 1) + ")";
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
  for (const std::string& segment : segments) {
    if (current->is_array()) {
      throw Error(display_pointer(prefix), "-p will not create or grow arrays",
                  "append the element first with the '-' sentinel, "
                  "e.g. jtSet " + display_pointer(prefix) + "/- '{}'");
    }
    if (!current->is_object()) {
      throw Error(display_pointer(prefix),
                  "cannot create '" + segment + "' inside a " + type_name(*current),
                  "remove or replace that value first");
    }
    if (!current->contains(segment)) {
      current->set(segment, jsom::JsonDocument::make_object());
    }
    current = &(*current)[segment];
    prefix += "/" + jsom::JsonPointer::escape_segment(segment);
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
