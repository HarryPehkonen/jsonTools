#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/move.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtMove <from> <to> [--if-not-set | --replace] [--pretty]";
}

int main(int argc, char* argv[]) {
  jt::GlobalArgs globals;
  jt::DestMode mode = jt::DestMode::Overwrite;
  bool mode_set = false;
  std::vector<std::string> positional;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (jt::take_global(arg, "jtMove", kUsage, globals)) {
      if (globals.exit_now) return 0;
    } else if (arg == "--if-not-set" || arg == "--replace") {
      if (mode_set) jt::fail("<args>", "--if-not-set and --replace are exclusive", kUsage);
      mode = arg == "--if-not-set" ? jt::DestMode::IfNotSet : jt::DestMode::Replace;
      mode_set = true;
    } else {
      positional.push_back(arg);
    }
  }

  if (positional.size() != 2) jt::fail("<args>", "expected <from> and <to>", kUsage);

  return jt::run_cli([&] {
    jsom::JsonDocument doc = jt::read_stdin();
    doc = jt::move(std::move(doc), positional[0], positional[1], mode);
    jt::write_stdout(doc, globals.pretty);
  });
}
