#include "jt/args.hpp"

#include "jt/errors.hpp"

#include <cstdlib>
#include <iostream>

namespace jt {

bool take_global(const std::string& arg, const char* tool, const char* usage,
                 GlobalArgs& globals) {
  if (arg == "--pretty") {
    globals.pretty = true;
    return true;
  }
  if (arg == "--help" || arg == "-h") {
    std::cout << "usage: " << usage << "\n";
    globals.exit_now = true;
    return true;
  }
  if (arg == "--version") {
    std::cout << tool << " (jsonTools) 0.1.0\n";
    globals.exit_now = true;
    return true;
  }
  return false;
}

std::string option_value(int argc, char* argv[], int& index, const char* option) {
  if (index + 1 >= argc) {
    fail("<args>", std::string(option) + " needs a value");
  }
  return argv[++index];
}

long long option_number(int argc, char* argv[], int& index, const char* option) {
  std::string text = option_value(argc, argv, index, option);
  try {
    std::size_t used = 0;
    long long value = std::stoll(text, &used);
    if (used != text.size() || value < 0) throw std::invalid_argument("");
    return value;
  } catch (const std::exception&) {
    fail("<args>", std::string(option) + " expects a non-negative integer, got '" +
                       text + "'");
  }
}

} // namespace jt
