#pragma once

#include <jsom/jsom.hpp>
#include <jsom/json_pointer.hpp>

#include <string>
#include <vector>

namespace jt {

// jsonTools spells the whole document "/" (REQUIREMENTS §4 default listPath,
// §12 "jtGet / = identity"); RFC 6901 spells the root "". Both spellings are
// accepted and normalized to "" here, so "/" never means the empty-string key.
// Every other pointer must be a well-formed absolute RFC 6901 pointer.
// Throws jt::Error on a relative or malformed pointer.
std::string normalize_pointer(const std::string& path);

// How a pointer is written in an error message ("" reads as "/").
std::string display_pointer(const std::string& pointer);

// Navigate, or throw jt::Error("path not found") with a typo hint.
const jsom::JsonDocument& require_at(const jsom::JsonDocument& doc,
                                     const std::string& pointer);

// "did you mean /user/name?" for a pointer that failed to resolve; "" when no
// existing key is a plausible typo of the failing segment.
std::string suggest_for(const jsom::JsonDocument& doc, const std::string& pointer);

// JSON type name: null / boolean / number / string / object / array.
std::string type_name(const jsom::JsonDocument& doc);

// Throw jt::Error when the container that would hold `pointer`'s leaf is
// missing or cannot hold it: a scalar can hold nothing, and an array takes
// only an in-range index or the "-" append sentinel. `fallback_hint` is used
// when no existing key looks like a typo of the missing segment. A root
// pointer has no parent and always passes.
void require_parent(const jsom::JsonDocument& doc, const std::string& pointer,
                    const std::string& fallback_hint);

// Create every missing object along `pointer`, mkdir -p style. Never creates
// or grows an array, and never tunnels through a scalar (both throw).
void create_object_path(jsom::JsonDocument& doc, const std::string& pointer);

// Turn an element-relative key path ("name", "name/last") into a pointer
// ("/name", "/name/last"). Rejects a leading slash — per-element verbs take
// relative paths (REQUIREMENTS §4).
std::string relative_key_pointer(const std::string& key_path);

} // namespace jt
