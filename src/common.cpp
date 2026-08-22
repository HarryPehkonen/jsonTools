#include "jt/common.hpp"
#include "jt/errors.hpp"

#include <iostream>
#include <iterator>
#include <string>

namespace jt {

jsom::JsonDocument read_stdin() {
  std::string all((std::istreambuf_iterator<char>(std::cin)),
                  std::istreambuf_iterator<char>());
  try {
    return jsom::parse_document(all);
  } catch (const std::exception& e) {
    fail("<input>", "invalid JSON", e.what());
  }
}

void write_stdout(const jsom::JsonDocument& doc, bool pretty) {
  std::cout << doc.to_json(pretty) << "\n";
}

jsom::JsonDocument parse_literal(const std::string& text) {
  try {
    return jsom::parse_document(text);
  } catch (const std::exception& e) {
    fail("<literal>", "invalid JSON literal", e.what());
  }
}

} // namespace jt
