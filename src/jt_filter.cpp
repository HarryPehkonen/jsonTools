#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/filter.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage =
    "jtFilter [<listPath>] <keyPath> "
    "(--eq|--ne|--gt|--ge|--lt|--le) <literal> [--pretty]";
}

int main(int argc, char* argv[]) {
  jt::GlobalArgs globals;
  std::string list_path = "/";
  std::string key_path;
  bool have_key = false;
  jt::Op op = jt::Op::Eq;
  std::string literal;
  bool have_op = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    jt::Op parsed = jt::Op::Eq;
    if (jt::take_global(arg, "jtFilter", kUsage, globals)) {
      if (globals.exit_now) return 0;
    } else if (jt::op_from_flag(arg, parsed)) {
      if (have_op) jt::fail("<args>", "only one comparison is allowed", kUsage);
      op = parsed;
      literal = jt::option_value(argc, argv, i, arg.c_str());
      have_op = true;
    } else if (!have_key && list_path == "/" && !arg.empty() && arg[0] == '/') {
      // As with jtSort: the optional listPath is absolute, the key is not.
      list_path = arg;
    } else if (!have_key) {
      jt::reject_empty_positional(arg);
      key_path = arg;
      have_key = true;
    } else {
      jt::fail("<args>", "unexpected argument '" + arg + "'", kUsage);
    }
  }

  if (!have_key) jt::fail("<args>", "missing <keyPath>", kUsage);
  if (!have_op) jt::fail("<args>", "missing a comparison flag", kUsage);

  return jt::run_cli([&] {
    jsom::JsonDocument value = jt::parse_literal(literal);
    jsom::JsonDocument doc = jt::read_stdin();
    doc = jt::filter(std::move(doc), list_path, key_path, op, value);
    jt::write_stdout(doc, globals.pretty);
  });
}
