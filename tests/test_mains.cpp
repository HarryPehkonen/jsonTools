// The tool mains (review issue 12): an empty-string positional argument is a
// shell-quoting footgun — '' normalized to the root pointer, so 'jtSet "" 5'
// silently replaced the whole document. These tests spawn the real binaries
// with an exact argv (no shell in between, so "" stays a genuine empty
// argument) and pin the contract: every main rejects '' positionals, root
// stays spellable as '/', and jtNew with no arguments still defaults to {}.
#include <gtest/gtest.h>

#include "jt/version.hpp"

#include <csignal>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

struct RunResult {
    int exit_code = -1;
    std::string out;
    std::string err;
};

std::string read_all(int fd) {
    std::string text;
    char buffer[4096];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof buffer)) > 0) {
        text.append(buffer, static_cast<std::size_t>(n));
    }
    return text;
}

// Runs args[0] (an absolute path under JT_BIN_DIR) with the exact argv.
RunResult run_tool(const std::vector<std::string>& args, const std::string& input) {
    int in_pipe[2], out_pipe[2], err_pipe[2];
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        ADD_FAILURE() << "pipe() failed";
        return {};
    }
    // A tool that exits before reading stdin must not kill the test runner.
    std::signal(SIGPIPE, SIG_IGN);
    const pid_t pid = fork();
    if (pid < 0) {
        ADD_FAILURE() << "fork() failed";
        return {};
    }
    if (pid == 0) {
        std::signal(SIGPIPE, SIG_DFL);
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);
        std::vector<char*> argv;
        argv.reserve(args.size() + 1); // one per argument, plus the nullptr terminator
        for (const std::string& a : args)
            argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(argv[0], argv.data());
        _exit(127); // execv failed
    }
    close(in_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[1]);
    // Every input and output here is a few bytes, far below the pipe buffer,
    // so plain blocking I/O cannot deadlock.
    if (!input.empty() && write(in_pipe[1], input.data(), input.size()) < 0) {
        // EPIPE: the tool rejected its arguments before reading stdin.
    }
    close(in_pipe[1]);
    RunResult result;
    result.out = read_all(out_pipe[0]);
    result.err = read_all(err_pipe[0]);
    close(out_pipe[0]);
    close(err_pipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }
    if (result.exit_code == 127) {
        ADD_FAILURE() << "could not execute " << args[0] << " (is the tool built?)";
    }
    return result;
}

std::string tool(const char* name) { return std::string(JT_BIN_DIR) + "/" + name; }

std::string display(const std::vector<std::string>& args) {
    std::string text = args[0];
    for (std::size_t i = 1; i < args.size(); ++i) {
        text += " '" + args[i] + "'";
    }
    return text;
}

// review issue 12's exact example: 'jtSet "" 5' used to replace the whole
// document, because '' normalized to the root pointer.
TEST(JtMains, SetRejectsAnEmptyPath) {
    const RunResult r = run_tool({tool("jtSet"), "", "5"}, "{\"a\":1}\n");
    EXPECT_NE(r.exit_code, 0);
    EXPECT_EQ(r.out, "");
    EXPECT_NE(r.err.find("Error at <args>"), std::string::npos);
    EXPECT_NE(r.err.find("empty"), std::string::npos);
}

// Every main rejects an empty-string positional — whichever slot it lands
// in — with a message that names the problem. (jtSort/jtFilter already
// errored on '' via their key validation; the mains now reject it up front
// like the rest.)
TEST(JtMains, EmptyPositionalArgumentsAreRejected) {
    const std::vector<std::vector<std::string>> invocations = {
        {tool("jtGet"), ""},
        {tool("jtRemove"), ""},
        {tool("jtMove"), "", "/a"},
        {tool("jtMove"), "/a", ""},
        {tool("jtCopy"), "", "/a"},
        {tool("jtCopy"), "/a", ""},
        {tool("jtZip"), "", "/a"},
        {tool("jtZip"), "/a", ""},
        {tool("jtLen"), "", "/n"},
        {tool("jtLen"), "/a", ""},
        {tool("jtSelect"), ""},
        {tool("jtSelect"), "/a", ""},
        {tool("jtType"), ""},
        {tool("jtKeys"), ""},
        {tool("jtKeys"), "/a", ""},
        {tool("jtValues"), ""},
        {tool("jtValues"), "/a", ""},
        {tool("jtSort"), ""},
        {tool("jtFilter"), "", "--gt", "0"},
        {tool("jtSet"), "/k", ""},
        {tool("jtNew"), ""},
        {tool("jtFrom"), ""},
    };
    for (const std::vector<std::string>& invocation : invocations) {
        SCOPED_TRACE(display(invocation));
        const RunResult r = run_tool(invocation, "{\"a\":1}\n");
        EXPECT_NE(r.exit_code, 0);
        EXPECT_NE(r.err.find("empty"), std::string::npos);
    }
}

// The legitimate spellings issue 12 must not break: root is spelled '/' ...
TEST(JtMains, GetRootBySlashStillWorks) {
    const RunResult r = run_tool({tool("jtGet"), "/"}, "{\"a\":1}\n");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "{\"a\":1}\n");
}

// ... and jtNew with no arguments at all still defaults to an empty object.
TEST(JtMains, NewWithNoArgumentsStillWorks) {
    const RunResult r = run_tool({tool("jtNew")}, "");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "{}\n");
}

// --version output comes from the single source of truth (FIX_ME A.2): the
// string include/jt/version.hpp hands out is the string the binary prints,
// so a bump there cannot drift from what the tools report.
TEST(JtMains, VersionOutputComesFromVersionHeader) {
    const RunResult r = run_tool({tool("jtSet"), "--version"}, "");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find(jt::JT_VERSION), std::string::npos);
}

} // namespace
