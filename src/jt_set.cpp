#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/set.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtSet <path> <literal> [-p] [--pretty]";
}

int main(int argc, char* argv[]) {
  jt::GlobalArgs globals;
  bool mkdir_p = false;
  std::vector<std::string> positional;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (jt::take_global(arg, "jtSet", kUsage, globals)) {
      if (globals.exit_now) return 0;
    } else if (arg == "-p") {
      mkdir_p = true;
    } else {
      positional.push_back(arg);
    }
  }

  if (positional.size() != 2) {
    jt::fail("<args>", "expected <path> and <literal>", kUsage);
  }

  return jt::run_cli([&] {
    jsom::JsonDocument literal = jt::parse_literal(positional[1]);
    jsom::JsonDocument doc = jt::read_stdin();
    doc = jt::set(std::move(doc), positional[0], literal, mkdir_p);
    jt::write_stdout(doc, globals.pretty);
  });
}
