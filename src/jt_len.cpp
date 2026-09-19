#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/len.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtLen <listPath> <destPath> [-p] [--pretty]";
}

int main(int argc, char* argv[]) {
    jt::GlobalArgs globals;
    bool mkdir_p = false;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (jt::take_global(arg, "jtLen", kUsage, globals)) {
            if (globals.exit_now)
                return 0;
        } else if (arg == "-p") {
            mkdir_p = true;
        } else {
            jt::reject_empty_positional(arg);
            positional.push_back(arg);
        }
    }

    if (positional.size() != 2) {
        jt::fail("<args>", "expected <listPath> and <destPath>", kUsage);
    }

    return jt::run_cli([&] {
        jsom::JsonDocument doc = jt::read_stdin();
        doc = jt::len(std::move(doc), positional[0], positional[1], mkdir_p);
        jt::write_stdout(doc, globals.pretty);
    });
}
