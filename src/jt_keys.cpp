#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/keys.hpp"

#include <string>
#include <vector>

namespace {
const char* kUsage = "jtKeys [<objPath>] | jtKeys <objPath> <destPath> [-p] [--pretty]";
}

int main(int argc, char* argv[]) {
    jt::GlobalArgs globals;
    bool mkdir_p = false;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (jt::take_global(arg, "jtKeys", kUsage, globals)) {
            if (globals.exit_now)
                return 0;
        } else if (arg == "-p") {
            mkdir_p = true;
        } else {
            jt::reject_empty_positional(arg);
            positional.push_back(arg);
        }
    }

    return jt::run_cli([&] {
        jsom::JsonDocument doc = jt::read_stdin();
        if (positional.empty() || positional.size() == 1) {
            // Reduction: document becomes the keys array.
            doc = jt::keys(doc, positional.empty() ? "/" : positional[0]);
        } else if (positional.size() == 2) {
            // Form B: write the keys array to dest, document intact.
            doc = jt::keys_to(std::move(doc), positional[0], positional[1], mkdir_p);
        } else {
            jt::fail("<args>", "expected at most <objPath> <destPath>", kUsage);
        }
        jt::write_stdout(doc, globals.pretty);
    });
}
