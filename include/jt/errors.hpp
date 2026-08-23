#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace jt {

// Thrown by every library verb. Carries the three parts of the error model so
// the CLI can render `Error at <path>: <problem>. <suggestion>` while an
// in-process caller can inspect them. Library code never exits the process.
class Error : public std::runtime_error {
 public:
  Error(std::string path, std::string problem, std::string suggestion = "")
      : std::runtime_error(format(path, problem, suggestion)),
        path_(std::move(path)),
        problem_(std::move(problem)),
        suggestion_(std::move(suggestion)) {}

  const std::string& path() const { return path_; }
  const std::string& problem() const { return problem_; }
  const std::string& suggestion() const { return suggestion_; }

  static std::string format(const std::string& path, const std::string& problem,
                            const std::string& suggestion) {
    std::string out = "Error at " + path + ": " + problem;
    if (!suggestion.empty()) out += ". " + suggestion;
    return out;
  }

 private:
  std::string path_, problem_, suggestion_;
};

// Emit `Error at <path>: <problem>. <suggestion>` and exit(1).
[[noreturn]] void fail(const std::string& path,
                       const std::string& problem,
                       const std::string& suggestion = "");

// Compute a "did you mean /x?" hint against a set of candidate sibling keys.
// Returns "" when no candidate is close enough.
std::string nearest_key(const std::vector<std::string>& candidates,
                        const std::string& target);

// The raw key behind nearest_key(), for callers that need to build a full
// pointer rather than a bare "/key". Returns "" when nothing is close enough.
std::string nearest_candidate(const std::vector<std::string>& candidates,
                              const std::string& target);

// CLI shim: run `body`, and turn a jt::Error (or any other exception) into the
// standard one-line stderr message plus exit(1). Every jt* main() wraps its
// work in this so the exit path lives in exactly one place.
template <typename Body>
int run_cli(Body&& body) {
  try {
    std::forward<Body>(body)();
  } catch (const Error& e) {
    fail(e.path(), e.problem(), e.suggestion());
  } catch (const std::exception& e) {
    fail("<internal>", e.what());
  }
  return 0;
}

} // namespace jt
