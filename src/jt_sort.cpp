#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/sort.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage =
    "jtSort [<listPath>] [<key> ...] [--desc-for <key>] [--pretty]";
}

int main(int argc, char* argv[]) {
  jt::GlobalArgs globals;
  std::string list_path = "/";
  std::vector<jt::SortKey> keys;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (jt::take_global(arg, "jtSort", kUsage, globals)) {
      if (globals.exit_now) return 0;
    } else if (arg == "--desc-for") {
      // Applies to the next single key only; repeat it for more (§12).
      keys.push_back({jt::option_value(argc, argv, i, "--desc-for"), true});
    } else if (keys.empty() && list_path == "/" && !arg.empty() && arg[0] == '/') {
      // The optional leading listPath is the only absolute pointer jtSort
      // takes; sort keys are element-relative, so they never start with '/'.
      list_path = arg;
    } else {
      jt::reject_empty_positional(arg);
      keys.push_back({arg, false});
    }
  }

  return jt::run_cli([&] {
    jsom::JsonDocument doc = jt::read_stdin();
    doc = jt::sort(std::move(doc), list_path, keys);
    jt::write_stdout(doc, globals.pretty);
  });
}
