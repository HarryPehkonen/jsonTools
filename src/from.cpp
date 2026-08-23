#include "jt/from.hpp"

#include "jt/errors.hpp"

#include <jsom/json_pointer.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

namespace jt {
namespace {

// Walks the parsed tree looking for the first container deeper than `limit`,
// reporting its pointer in `hit`. The root's pointer is "", so the answer
// cannot be carried by the string alone.
bool first_too_deep(const jsom::JsonDocument& node, const std::string& pointer,
                    int depth, int limit, std::string& hit) {
  if (!node.is_object() && !node.is_array()) return false;
  if (depth > limit) {
    hit = pointer;
    return true;
  }
  if (node.is_object()) {
    for (const auto& entry : node.items()) {
      std::string child =
          pointer + "/" + jsom::JsonPointer::escape_segment(entry.first);
      if (first_too_deep(entry.second, child, depth + 1, limit, hit)) return true;
    }
  } else {
    const auto& arr = node.as_array();
    for (std::size_t i = 0; i < arr.size(); ++i) {
      std::string child = pointer + "/" + std::to_string(i);
      if (first_too_deep(arr[i], child, depth + 1, limit, hit)) return true;
    }
  }
  return false;
}

// A second, minimal pass over the raw text: JSOM's parser keeps the last value
// for a repeated key without complaining, so the only way to see duplicates is
// to look at the source. The text is known to be well-formed by the time this
// runs, which keeps the scanner small.
class DuplicateScanner {
 public:
  DuplicateScanner(const std::string& text, std::vector<std::string>& warnings)
      : text_(text), warnings_(warnings) {}

  void run() { value(""); }

 private:
  void skip_ws() {
    while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                                   text_[pos_] == '\n' || text_[pos_] == '\r')) {
      ++pos_;
    }
  }

  char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

  // Consumes a JSON string starting at the opening quote, honoring escapes so
  // that braces and quotes inside strings never reach the structural logic.
  std::string string_token() {
    std::string out;
    ++pos_; // opening quote
    while (pos_ < text_.size() && text_[pos_] != '"') {
      if (text_[pos_] == '\\' && pos_ + 1 < text_.size()) {
        // Keep the escape verbatim: key identity only needs to be consistent.
        out += text_[pos_];
        out += text_[pos_ + 1];
        pos_ += 2;
        continue;
      }
      out += text_[pos_++];
    }
    ++pos_; // closing quote
    return out;
  }

  void value(const std::string& pointer) {
    skip_ws();
    if (peek() == '{') {
      object(pointer);
    } else if (peek() == '[') {
      array(pointer);
    } else if (peek() == '"') {
      string_token();
    } else {
      while (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '}' &&
             text_[pos_] != ']' && text_[pos_] != ' ' && text_[pos_] != '\t' &&
             text_[pos_] != '\n' && text_[pos_] != '\r') {
        ++pos_;
      }
    }
  }

  void object(const std::string& pointer) {
    ++pos_; // '{'
    std::set<std::string> seen;
    skip_ws();
    if (peek() == '}') {
      ++pos_;
      return;
    }
    while (pos_ < text_.size()) {
      skip_ws();
      std::string key = string_token();
      std::string child =
          pointer + "/" + jsom::JsonPointer::escape_segment(key);
      if (!seen.insert(key).second) {
        warnings_.push_back("duplicate key at " + child);
      }
      skip_ws();
      ++pos_; // ':'
      value(child);
      skip_ws();
      if (peek() == ',') {
        ++pos_;
        continue;
      }
      ++pos_; // '}'
      return;
    }
  }

  void array(const std::string& pointer) {
    ++pos_; // '['
    skip_ws();
    if (peek() == ']') {
      ++pos_;
      return;
    }
    std::size_t index = 0;
    while (pos_ < text_.size()) {
      value(pointer + "/" + std::to_string(index++));
      skip_ws();
      if (peek() == ',') {
        ++pos_;
        continue;
      }
      ++pos_; // ']'
      return;
    }
  }

  const std::string& text_;
  std::vector<std::string>& warnings_;
  std::size_t pos_ = 0;
};

} // namespace

int document_depth(const jsom::JsonDocument& doc) {
  if (!doc.is_object() && !doc.is_array()) return 0;
  int deepest = 0;
  if (doc.is_object()) {
    for (const auto& entry : doc.items()) {
      deepest = std::max(deepest, document_depth(entry.second));
    }
  } else {
    for (const auto& element : doc.as_array()) {
      deepest = std::max(deepest, document_depth(element));
    }
  }
  return deepest + 1;
}

FromResult from_text(const std::string& text, const std::string& origin,
                     const FromOptions& opts) {
  if (opts.max_size >= 0 &&
      static_cast<long long>(text.size()) > opts.max_size) {
    throw Error(origin,
                "input is " + std::to_string(text.size()) + " bytes, over the " +
                    std::to_string(opts.max_size) + "-byte limit",
                "raise --max-size or shrink the input");
  }

  FromResult result{jsom::JsonDocument(), {}};
  try {
    result.doc = jsom::parse_document(text);
  } catch (const std::exception& e) {
    throw Error(origin, "invalid JSON", e.what());
  }

  if (opts.max_depth >= 0) {
    std::string too_deep;
    if (first_too_deep(result.doc, "", 1, opts.max_depth, too_deep)) {
      throw Error(too_deep.empty() ? origin : too_deep,
                  "nesting exceeds the --max-depth limit of " +
                      std::to_string(opts.max_depth),
                  "raise --max-depth or flatten the input");
    }
  }

  if (opts.warn_duplicates) {
    DuplicateScanner(text, result.warnings).run();
  }
  return result;
}

FromResult from_file(const std::string& file, const FromOptions& opts) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    throw Error(file, "cannot open file", "check the path and permissions");
  }
  std::string text((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  return from_text(text, file, opts);
}

} // namespace jt
