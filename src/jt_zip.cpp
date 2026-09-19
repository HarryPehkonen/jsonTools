#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/zip.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtZip <keysPath> <valuesPath> [--overwrite] [--pretty]";
}

int main(int argc, char* argv[]) {
    jt::GlobalArgs globals;
    bool overwrite = false;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (jt::take_global(arg, "jtZip", kUsage, globals)) {
            if (globals.exit_now)
                return 0;
        } else if (arg == "--overwrite") {
            overwrite = true;
        } else {
            jt::reject_empty_positional(arg);
            positional.push_back(arg);
        }
    }

    if (positional.size() != 2) {
        jt::fail("<args>", "expected <keysPath> and <valuesPath>", kUsage);
    }

    return jt::run_cli([&] {
        jsom::JsonDocument doc = jt::read_stdin();
        doc = jt::zip(doc, positional[0], positional[1], overwrite);
        jt::write_stdout(doc, globals.pretty);
    });
}
