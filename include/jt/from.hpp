#pragma once

#include <jsom/jsom.hpp>

#include <string>
#include <vector>

namespace jt {

struct FromOptions {
  // Emit nothing; validate only. (Purely a CLI concern — the library still
  // returns the parsed document so callers can assert on it.)
  bool check_only = false;
  // Maximum container nesting depth; -1 disables the cap.
  int max_depth = -1;
  // Maximum file size in bytes; -1 disables the cap.
  long long max_size = -1;
  // Collect a warning for every repeated key within one object.
  bool warn_duplicates = false;
};

struct FromResult {
  jsom::JsonDocument doc;
  std::vector<std::string> warnings;
};

// Read `file` and validate it before it enters the pipeline. Throws jt::Error
// on an unreadable file, a syntax error, or a violated cap.
FromResult from_file(const std::string& file, const FromOptions& opts);

// Same validations against text already in memory. `origin` names the source
// in error messages.
FromResult from_text(const std::string& text, const std::string& origin,
                     const FromOptions& opts);

// Container nesting depth: 0 for a scalar, 1 for an empty container,
// 1 + deepest child otherwise.
int document_depth(const jsom::JsonDocument& doc);

} // namespace jt
