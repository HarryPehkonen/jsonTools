#pragma once

#include <jsom/jsom.hpp>

#include <cstdint>
#include <string>

namespace jt {

// The comparison a filter applies, mirroring the --eq/--ne/--gt/--ge/--lt/--le
// CLI flags. One byte on purpose: six values, and this enum rides along on every
// element the filter examines.
enum class Op : std::uint8_t { Eq, Ne, Gt, Ge, Lt, Le };

// Map a CLI flag ("--gt") onto an operator. Returns false for anything else.
bool op_from_flag(const std::string& flag, Op& out);

// Keep the elements of the list at `list_path` (default "/", the whole
// document) whose value at the element-relative `key_path` satisfies
// `op` against `value`. An element missing the key is dropped silently; an
// ordering comparison across two different types is an error.
jsom::JsonDocument filter(jsom::JsonDocument doc, const std::string& list_path,
                          const std::string& key_path, Op op, const jsom::JsonDocument& value);

} // namespace jt
