#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/select.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtSelect <path> [<path> ...] [--pretty]";
}

int main(int argc, char* argv[]) {
    jt::GlobalArgs globals;
    std::vector<std::string> paths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (jt::take_global(arg, "jtSelect", kUsage, globals)) {
            if (globals.exit_now)
                return 0;
        } else {
            jt::reject_empty_positional(arg);
            paths.push_back(arg);
        }
    }

    if (paths.empty())
        jt::fail("<args>", "expected at least one <path>", kUsage);

    return jt::run_cli([&] {
        jsom::JsonDocument doc = jt::read_stdin();
        doc = jt::select(std::move(doc), paths);
        jt::write_stdout(doc, globals.pretty);
    });
}
