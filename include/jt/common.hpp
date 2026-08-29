#pragma once

#include <jsom/jsom.hpp>
#include <string>

namespace jt {

// Read all of stdin and parse it as JSON. On failure, emit an error and exit.
jsom::JsonDocument read_stdin();

// Write a document to stdout, honoring the pretty flag.
void write_stdout(const jsom::JsonDocument& doc, bool pretty);

// Parse a JSON literal argument (used by jtSet/jtGet/--default).
jsom::JsonDocument parse_literal(const std::string& text);

} // namespace jt
