// jt_fuzz — libFuzzer over jsonTools' OWN parsing surface.
//
// JSOM owns JSON text parsing and already fuzzes it (JSOM/tests/fuzzer.cpp,
// fuzz_jsom). What belongs to jsonTools is the layer on top of the parser, and
// that is what this target drives:
//
//   * RFC 6901 pointer strings — jt::normalize_pointer, display_pointer,
//     relative_key_pointer;
//   * the traversal that turns a pointer into a document position —
//     require_at, require_parent, create_object_path (the '-' append sentinel,
//     ~0/~1 unescaping, array-index classification, the to_array_index
//     overflow path);
//   * the write verbs built on it — set (mkdir -p on and off), copy and move
//     (all three DestMode values), remove;
//   * the argv helpers every tool shares — take_global.
//
// Input format: NUL-separated fields.
//   field 0    the document, as JSON text. It is used when it looks like a
//              container ('{' or '['); otherwise the built-in document below
//              is used, so a pointer-only input — everything the dictionary
//              produces on its own — still meets nested objects, an array, an
//              empty array, an empty object, a scalar and keys that need
//              ~0/~1 escaping.
//   field 1+   candidate pointers. With a single field that field is the
//              pointer. The document's own list_paths() are added, so the
//              write verbs also run on pointers that already resolve.
//
// Rejecting an input is an expected outcome, never a failure: each call that
// can reject is wrapped in its own narrow catch, and the ORACLE runs OUTSIDE
// those catches, so a throw from a property is itself a finding. The libFuzzer
// crash artifact is the whole input, and every decision here is a pure
// function of those bytes, so a finding reproduces with:
//
//   ./build-fuzz/fuzz_jt crash-<hash>
//
// Deliberately NOT driven in-process: jt::option_value, jt::option_number and
// jt::reject_empty_positional. They report a bad argument by calling fail(),
// i.e. std::exit(1) (errors.hpp — the CLI error path), so the first rejected
// argument would end the campaign. Forking a child for them was tried and
// rejected: under -fsanitize=fuzzer,address a forked child that hits an ASan
// error exits 1, exactly like an expected rejection does, so the parent sees
// "no crash" and the gate would pass on a real finding (verified 2026-09-19).
// Their inputs are string compares and one std::stoll — no memory surface.
// The acceptance contract (including FIX_ME A.4's "+5") belongs in tests/,
// where the real binaries run with an exact argv; see tests/test_mains.cpp.

#include "jt/args.hpp"
#include "jt/copy.hpp"
#include "jt/errors.hpp"
#include "jt/move.hpp"
#include "jt/paths.hpp"
#include "jt/remove.hpp"
#include "jt/set.hpp"

#include <jsom/jsom.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

namespace {

// Per-input work is capped (the campaign's budget is inputs, not bytes): the
// error paths are what run millions of times.
constexpr std::size_t MAX_INPUT = 4096;
constexpr std::size_t MAX_FIELDS = 8;
constexpr std::size_t MAX_FIELD = 256;
// Each pointer costs ~80 library calls, most of them the (expensive) error
// path, so this is the knob that sets the campaign's input rate: 60 s of gate
// smoke is tens of thousands of inputs, a night is millions.
constexpr std::size_t MAX_POINTERS = 4;
constexpr int MAX_LIST_DEPTH = 3;

constexpr std::array<const char*, 6> LITERALS = {"42", "\"text\"", "true", "null", "{}", "[]"};

[[noreturn]] void property_violated(const char* property, const std::string& detail) {
    std::cerr << "FUZZ PROPERTY VIOLATED: " << property << "\n"
              << "  " << detail << "\n";
    std::abort();
}

// jt::take_global prints for --help/--version — that is its contract — and
// mutated inputs hit those spellings often enough that stdout would be mostly
// version strings; on a failure that is what the gate's tail would show instead
// of the crash report. Point stdout at a sink while driving it.
class MutedStdout {
public:
    MutedStdout() : saved_(std::cout.rdbuf(&sink_)) {}
    ~MutedStdout() { std::cout.rdbuf(saved_); }
    MutedStdout(const MutedStdout&) = delete;
    MutedStdout& operator=(const MutedStdout&) = delete;

private:
    class Sink : public std::streambuf {
    protected:
        int overflow(int c) override { return c; }
    };
    Sink sink_;
    std::streambuf* saved_;
};

// Runs `body` and reports whether the library accepted the input. Narrow on
// purpose: jt::Error is the documented rejection path and JSOM's two pointer
// and type exceptions are the same thing one level down; anything else
// escapes to libFuzzer, which records an uncaught exception as a crash.
template <typename Body> bool accepted(Body&& body) {
    try {
        body();
        return true;
    } catch (const jt::Error&) {
        return false;
    } catch (const jsom::JsonPointerException&) {
        return false;
    } catch (const jsom::TypeException&) {
        return false;
    }
}

std::vector<std::string> split_fields(const std::uint8_t* data, std::size_t size) {
    std::vector<std::string> fields;
    std::string current;
    for (std::size_t i = 0; i < size; ++i) {
        if (data[i] == '\0') {
            fields.push_back(current);
            current.clear();
            if (fields.size() >= MAX_FIELDS)
                return fields;
        } else if (current.size() < MAX_FIELD) {
            current.push_back(static_cast<char>(data[i]));
        }
    }
    fields.push_back(current);
    return fields;
}

bool looks_like_container(const std::string& text) {
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            continue;
        return c == '{' || c == '[';
    }
    return false;
}

// Every shape the traversal classifies: nested objects, an array, an empty
// array, an empty object, a scalar under a key, the root, an empty key and
// keys holding '/' and '~' (so the escaped spellings get exercised).
jsom::JsonDocument default_document() {
    return jsom::parse_document(
        R"({"a":{"b":{"c":1},"list":[1,2,3]},"items":[{"name":"one"},{"name":"two"}],)"
        R"("empty_obj":{},"empty_arr":[],"":0,"a/b":1,"a~b":2,"scalar":"text"})");
}

jsom::JsonDocument document_for(const std::vector<std::string>& fields) {
    if (!fields.empty() && looks_like_container(fields[0])) {
        try {
            return jsom::parse_document(fields[0]);
        } catch (const std::exception&) {
            // Unparseable container text is just another input: fall back to the
            // built-in document rather than leaving the input undriven.
            return default_document();
        }
    }
    return default_document();
}

std::vector<std::string> candidate_pointers(const std::vector<std::string>& fields,
                                            const jsom::JsonDocument& doc) {
    std::vector<std::string> pointers;
    pointers.push_back(fields.size() > 1 ? fields[1] : fields[0]);
    for (std::size_t i = 2; i < fields.size() && pointers.size() < MAX_POINTERS; ++i)
        pointers.push_back(fields[i]);
    // Paths the document itself lists: random bytes make a resolving pointer
    // only by accident, and the write verbs are where the logic is.
    if (pointers.size() < MAX_POINTERS && (doc.is_object() || doc.is_array())) {
        const std::vector<std::string> own = doc.list_paths(MAX_LIST_DEPTH);
        for (const std::string& path : own) {
            if (pointers.size() >= MAX_POINTERS)
                break;
            pointers.push_back(path);
        }
    }
    return pointers;
}

// The read traversal, and the mkdir -p path that creates the intermediates.
void drive_traversal(const jsom::JsonDocument& doc, const std::string& norm) {
    accepted([&] { (void)jt::require_at(doc, norm); });
    accepted([&] { jt::require_parent(doc, norm, "hint"); });
    (void)jt::suggest_for(doc, norm);
    if (doc.is_object())
        (void)jt::nearest_key(doc.keys(), norm);

    // create_object_path is mkdir -p: once it has created the intermediates,
    // running it again must be a no-op on the same document.
    jsom::JsonDocument created = doc;
    if (accepted([&] { jt::create_object_path(created, norm); })) {
        jsom::JsonDocument again = created;
        if (!accepted([&] { jt::create_object_path(again, norm); }))
            property_violated("create_object_path is idempotent (second call accepted)",
                              "pointer: " + norm);
        if (!(again == created))
            property_violated("create_object_path is idempotent (second call changes nothing)",
                              "pointer: " + norm);
    }
}

void drive_set(const jsom::JsonDocument& doc, const std::string& path, const std::string& norm,
               const jsom::JsonDocument& literal) {
    // The append sentinel has no readable spelling: it writes at a new index by
    // definition, so only the non-append pointers carry the round-trip property.
    if (!norm.empty() && jsom::JsonPointer::is_append(jsom::JsonPointer::get_last_segment(norm)))
        return;

    for (const bool mkdir_p : {false, true}) {
        jsom::JsonDocument written;
        if (!accepted([&] { written = jt::set(doc, path, literal, mkdir_p); }))
            continue;
        const jsom::JsonDocument* read_back = nullptr;
        if (!accepted([&] { read_back = &jt::require_at(written, norm); }))
            property_violated("set writes a pointer require_at can read again", "pointer: " + norm);
        if (!(*read_back == literal))
            property_violated("set round-trip (the value read back is the literal)",
                              "pointer: " + norm);
    }
}

void drive_remove(const jsom::JsonDocument& doc, const std::string& path, const std::string& norm) {
    if (norm.empty())
        return; // the root cannot be removed, and remove() rejects it
    jsom::JsonDocument out;
    if (!accepted([&] { out = jt::remove(doc, path); }))
        return;

    const std::string parent = jsom::JsonPointer::get_parent(norm);
    const jsom::JsonDocument* before = doc.find(parent);
    const jsom::JsonDocument* after = out.find(parent);
    if (before == nullptr || after == nullptr)
        property_violated("remove keeps the parent container", "pointer: " + norm);
    // One element, object key or array slot: both shapes shrink by exactly one.
    if (after->size() + 1 != before->size())
        property_violated("remove shrinks its parent container by exactly one", "pointer: " + norm);
}

// "from: <src> to: <dst>": the two-path context every copy/move property reports.
// Built by appending — clang-tidy's performance-inefficient-string-concatenation
// is right about the chained form, and this runs on the failure path only.
std::string pair_note(const std::string& src, const std::string& dst) {
    std::string note = "from: ";
    note += src;
    note += " to: ";
    note += dst;
    return note;
}

// copy/move take two pointers, so each candidate is also paired with the next.
// DestMode is part of the contract: a blocked destination is a condition, not
// an error, and leaves the document byte-for-byte alone.
void drive_pair(const jsom::JsonDocument& doc, const std::string& from, const std::string& to) {
    std::string src, dst;
    if (!accepted([&] { src = jt::normalize_pointer(from); })
        || !accepted([&] { dst = jt::normalize_pointer(to); }))
        return;

    const jsom::JsonDocument* value = nullptr;
    if (!accepted([&] { value = &jt::require_at(doc, src); }))
        return; // nothing to copy or move from
    const bool append
        = !dst.empty() && jsom::JsonPointer::is_append(jsom::JsonPointer::get_last_segment(dst));
    // Only move guards "into its own child", so copy accepts two overlapping
    // spellings that legitimately rewrite the source side: a destination inside
    // the source (jtCopy / /a nests the document inside itself) and a
    // destination that contains the source (an ancestor of it, which overwrites
    // the path the source lives at). The source checks only apply when the two
    // paths are unrelated.
    const bool overlapping
        = src != dst
          && (jsom::JsonPointer::is_prefix(src, dst) || jsom::JsonPointer::is_prefix(dst, src));

    for (const jt::DestMode mode :
         {jt::DestMode::Overwrite, jt::DestMode::IfNotSet, jt::DestMode::Replace}) {
        const bool allowed = jt::dest_write_allowed(doc, dst, mode);

        jsom::JsonDocument copied;
        if (accepted([&] { copied = jt::copy(doc, from, to, mode); })) {
            if (!allowed) {
                if (!(copied == doc))
                    property_violated("copy with a blocked destination changes nothing",
                                      pair_note(src, dst));
            } else {
                if (!overlapping) {
                    const jsom::JsonDocument* still = nullptr;
                    if (!accepted([&] { still = &jt::require_at(copied, src); }))
                        property_violated("copy keeps the source readable", pair_note(src, dst));
                    if (!(*still == *value))
                        property_violated("copy keeps the source's value", pair_note(src, dst));
                }
                if (!append) {
                    const jsom::JsonDocument* written = nullptr;
                    if (!accepted([&] { written = &jt::require_at(copied, dst); }))
                        property_violated("copy writes a pointer require_at can read",
                                          "to: " + dst);
                    if (!(*written == *value))
                        property_violated("copy writes the source's value at the destination",
                                          pair_note(src, dst));
                }
            }
        }

        jsom::JsonDocument moved;
        if (!accepted([&] { moved = jt::move(doc, from, to, mode); }))
            continue;
        if (!allowed) {
            if (!(moved == doc))
                property_violated("move with a blocked destination changes nothing",
                                  pair_note(src, dst));
        } else if (!append) {
            const jsom::JsonDocument* written = nullptr;
            if (!accepted([&] { written = &jt::require_at(moved, dst); }))
                property_violated("move writes a pointer require_at can read", "to: " + dst);
            if (!(*written == *value))
                property_violated("move carries the source's value to the destination",
                                  pair_note(src, dst));
        }
    }
}

void drive_pointer(const jsom::JsonDocument& doc, const std::string& path,
                   const jsom::JsonDocument& literal) {
    (void)jt::display_pointer(path); // total: no rejection to observe

    std::string norm;
    const bool absolute = accepted([&] { norm = jt::normalize_pointer(path); });
    std::string relative;
    if (accepted([&] { relative = jt::relative_key_pointer(path); })) {
        // The per-element spelling is contractually an absolute RFC 6901
        // pointer: "/" + the key path, with ~0/~1 escaping.
        if (relative.empty() || relative[0] != '/' || !jsom::JsonPointer::is_valid(relative))
            property_violated("relative_key_pointer yields an absolute RFC 6901 pointer",
                              "in: " + path + " out: " + relative);
    }
    if (!absolute)
        return; // the pointer itself was rejected, and that is the input's answer

    drive_traversal(doc, norm);
    drive_set(doc, path, norm, literal);
    drive_remove(doc, path, norm);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0 || size > MAX_INPUT)
        return 0;

    const std::vector<std::string> fields = split_fields(data, size);
    const jsom::JsonDocument doc = document_for(fields);
    const std::vector<std::string> pointers = candidate_pointers(fields, doc);
    const jsom::JsonDocument literal
        = jsom::parse_document(LITERALS[data[size - 1] % LITERALS.size()]);

    // argv: the globals every tool shares. The three helpers that exit the
    // process on a bad argument are covered by tests/, not here — see the top.
    jt::GlobalArgs globals;
    {
        const MutedStdout muted;
        for (const std::string& field : fields)
            (void)jt::take_global(field, "jtFuzz", "usage: jtFuzz [--pretty]", globals);
    }

    for (std::size_t i = 0; i < pointers.size(); ++i) {
        drive_pointer(doc, pointers[i], literal);
        drive_pair(doc, pointers[i], pointers[(i + 1) % pointers.size()]);
    }
    return 0;
}
