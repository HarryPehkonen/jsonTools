#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/keys.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtKeys [<path>] [--pretty]";
}

int main(int argc, char* argv[]) {
  jt::GlobalArgs globals;
  std::vector<std::string> positional;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (jt::take_global(arg, "jtKeys", kUsage, globals)) {
      if (globals.exit_now) return 0;
    } else {
      positional.push_back(arg);
    }
  }

  if (positional.size() > 1) {
    jt::fail("<args>", "expected at most one <path>", kUsage);
  }

  return jt::run_cli([&] {
    jsom::JsonDocument doc = jt::read_stdin();
    doc = jt::keys(std::move(doc), positional.empty() ? "/" : positional[0]);
    jt::write_stdout(doc, globals.pretty);
  });
}
