#include "jt/args.hpp"
#include "jt/common.hpp"
#include "jt/errors.hpp"
#include "jt/from.hpp"

#include <iostream>
#include <string>

namespace {
const char* kUsage = "jtFrom <file> [--check-only] [--max-depth N] [--max-size N] "
                     "[--warn-duplicates] [--pretty]";
}

int main(int argc, char* argv[]) {
    jt::GlobalArgs globals;
    jt::FromOptions opts;
    std::string file;
    bool have_file = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (jt::take_global(arg, "jtFrom", kUsage, globals)) {
            if (globals.exit_now)
                return 0;
        } else if (arg == "--check-only") {
            opts.check_only = true;
        } else if (arg == "--warn-duplicates") {
            opts.warn_duplicates = true;
        } else if (arg == "--max-depth") {
            opts.max_depth = static_cast<int>(jt::option_number(argc, argv, i, "--max-depth"));
        } else if (arg == "--max-size") {
            opts.max_size = jt::option_number(argc, argv, i, "--max-size");
        } else if (!have_file) {
            jt::reject_empty_positional(arg);
            file = arg;
            have_file = true;
        } else {
            jt::fail("<args>", "unexpected argument '" + arg + "'", kUsage);
        }
    }

    if (!have_file)
        jt::fail("<args>", "missing <file> argument", kUsage);

    return jt::run_cli([&] {
        jt::FromResult result = jt::from_file(file, opts);
        for (const std::string& warning : result.warnings) {
            std::cerr << "warning: " << warning << "\n";
        }
        if (!opts.check_only)
            jt::write_stdout(result.doc, globals.pretty);
    });
}
