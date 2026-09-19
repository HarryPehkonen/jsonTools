#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/get.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtGet <path> [--default <literal>] [--pretty]";
}

int main(int argc, char* argv[]) {
    jt::GlobalArgs globals;
    std::string default_text;
    bool have_default = false;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (jt::take_global(arg, "jtGet", kUsage, globals)) {
            if (globals.exit_now)
                return 0;
        } else if (arg == "--default") {
            default_text = jt::option_value(argc, argv, i, "--default");
            have_default = true;
        } else {
            jt::reject_empty_positional(arg);
            positional.push_back(arg);
        }
    }

    if (positional.size() != 1)
        jt::fail("<args>", "expected one <path>", kUsage);

    return jt::run_cli([&] {
        jsom::JsonDocument fallback
            = have_default ? jt::parse_literal(default_text) : jsom::JsonDocument();
        jsom::JsonDocument doc = jt::read_stdin();
        doc = jt::get(std::move(doc), positional[0], have_default ? &fallback : nullptr);
        jt::write_stdout(doc, globals.pretty);
    });
}
