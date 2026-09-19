// Include hygiene (FIX_ME B.6): the tool mains get jt::fail / jt::run_cli
// from jt/errors.hpp itself, never from what another header happens to pull
// in. The mid-file #includes below are the test: errors.hpp comes first,
// before anything else could provide its dependencies, and must still
// provide the whole CLI error machinery on its own; common.hpp and args.hpp
// follow and must declare what the mains call from them without leaning on a
// later header. (An audit of every translation unit in src/ and tests/
// found no current violator — the include lists are already explicit.)
#include "jt/errors.hpp"

#include <gtest/gtest.h>

TEST(JtIncludes, ErrorsHeaderAloneProvidesTheCliErrorMachinery) {
    // fail() exits the process, so pin it by address rather than calling it.
    EXPECT_NE(&jt::fail, nullptr);
    EXPECT_EQ(jt::Error::format("/a", "problem", "try this"), "Error at /a: problem. try this");
    EXPECT_EQ(jt::Error::format("/a", "problem", ""), "Error at /a: problem");
    bool ran = false;
    EXPECT_EQ(jt::run_cli([&] { ran = true; }), 0);
    EXPECT_TRUE(ran);
}

#include "jt/common.hpp"

TEST(JtIncludes, CommonHeaderDeclaresTheIoContract) {
    // read_stdin() would block for input and parse_literal() can exit(1);
    // the mains only need the declarations to compile against them.
    EXPECT_NE(&jt::read_stdin, nullptr);
    EXPECT_NE(&jt::write_stdout, nullptr);
    EXPECT_NE(&jt::parse_literal, nullptr);
}

#include "jt/args.hpp"

TEST(JtIncludes, ArgsHeaderDeclaresTheGlobalFlagContract) {
    EXPECT_NE(&jt::take_global, nullptr);
    EXPECT_NE(&jt::option_value, nullptr);
    EXPECT_NE(&jt::option_number, nullptr);
    jt::reject_empty_positional("non-empty passes untouched");
}
