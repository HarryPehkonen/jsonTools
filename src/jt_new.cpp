#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/version.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
  bool pretty = false;
  const char* literal = nullptr;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--pretty") pretty = true;
    else if (a == "--help" || a == "-h") {
      std::cout << "usage: jtNew [literal] [--pretty]\n";
      return 0;
    } else if (a == "--version") {
      std::cout << "jtNew (jsonTools) " << jt::JT_VERSION << "\n";
      return 0;
    } else if (literal == nullptr) {
      jt::reject_empty_positional(a);
      literal = argv[i];
    } else {
      std::cerr << "Error at <args>: unexpected argument '" << a << "'\n";
      return 1;
    }
  }

  // jtNew reads no stdin. A literal parses as-is (`jtNew '[]'` -> array);
  // no literal defaults to an empty object.
  jsom::JsonDocument doc =
      literal ? jt::parse_literal(literal) : jsom::parse_document("{}");

  jt::write_stdout(doc, pretty);
  return 0;
}
