#pragma once

#include <string>
#include <vector>

namespace jt {

// Emit `Error at <path>: <problem>. <suggestion>` and exit(1).
[[noreturn]] void fail(const std::string& path,
                       const std::string& problem,
                       const std::string& suggestion = "");

// Compute a "did you mean /x?" hint against a set of candidate sibling keys.
// Returns "" when no candidate is close enough.
std::string nearest_key(const std::vector<std::string>& candidates,
                        const std::string& target);

} // namespace jt
