#pragma once

#include <string>

namespace jt {

// Flags every jt* tool understands.
struct GlobalArgs {
  bool pretty = false;
  // Set when --help/--version printed; main() should return 0 immediately.
  bool exit_now = false;
};

// Consumes `--pretty`, `--help`/`-h` and `--version`. Returns true when the
// argument belonged to the global set and the tool's own parser should skip it.
bool take_global(const std::string& arg, const char* tool, const char* usage,
                 GlobalArgs& globals);

// Rejects a positional argument that is the empty string (review issue 12):
// '' reaching a tool is almost always a shell-quoting mistake, and for paths
// it silently meant the root. Root is spelled '/'.
void reject_empty_positional(const std::string& arg);

// Reads the value that follows an option, failing with a usable message when
// it is missing. `index` is advanced past the value.
std::string option_value(int argc, char* argv[], int& index, const char* option);

// Same, parsed as a non-negative integer.
long long option_number(int argc, char* argv[], int& index, const char* option);

} // namespace jt
