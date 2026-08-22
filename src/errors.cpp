#include "jt/errors.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace jt {

void fail(const std::string& path, const std::string& problem,
          const std::string& suggestion) {
  std::cerr << "Error at " << path << ": " << problem;
  if (!suggestion.empty()) {
    std::cerr << ". " << suggestion;
  }
  std::cerr << "\n";
  std::exit(1);
}

// Levenshtein distance (unbounded; keys are short).
static int edit_distance(const std::string& a, const std::string& b) {
  const size_t n = a.size(), m = b.size();
  std::vector<int> prev(m + 1), cur(m + 1);
  for (size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
  for (size_t i = 1; i <= n; ++i) {
    cur[0] = static_cast<int>(i);
    for (size_t j = 1; j <= m; ++j) {
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1,
                         prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
    }
    std::swap(prev, cur);
  }
  return prev[m];
}

std::string nearest_key(const std::vector<std::string>& candidates,
                        const std::string& target) {
  int best = -1;
  std::string best_key;
  for (const auto& c : candidates) {
    int d = edit_distance(c, target);
    // Only suggest when the key is plausibly a typo of the target.
    if (d >= 1 && d <= 2 && (best == -1 || d < best)) {
      best = d;
      best_key = c;
    }
  }
  return best_key.empty() ? "" : "did you mean /" + best_key + "?";
}

} // namespace jt
