// jtCopy — duplicate a value to another pointer, source retained.
#include <gtest/gtest.h>

#include "jt/copy.hpp"
#include "jt/errors.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) { return jsom::parse_document(text); }

TEST(JtCopy, CopiesAValueAndKeepsTheSource) {
    auto out = jt::copy(doc(R"({"a":1})"), "/a", "/b");
    EXPECT_EQ(out.to_json(), R"({"a":1,"b":1})");
}

TEST(JtCopy, DefaultOverwritesTheDestination) {
    auto out = jt::copy(doc(R"({"a":1,"b":2})"), "/a", "/b");
    EXPECT_EQ(out.to_json(), R"({"a":1,"b":1})");
}

TEST(JtCopy, IfNotSetLeavesAnExistingDestinationAlone) {
    auto out = jt::copy(doc(R"({"a":1,"b":2})"), "/a", "/b", jt::DestMode::IfNotSet);
    EXPECT_EQ(out.to_json(), R"({"a":1,"b":2})");
}

TEST(JtCopy, IfNotSetWritesAnAbsentDestination) {
    auto out = jt::copy(doc(R"({"a":1})"), "/a", "/b", jt::DestMode::IfNotSet);
    EXPECT_EQ(out.to_json(), R"({"a":1,"b":1})");
}

TEST(JtCopy, ReplaceOverwritesAnExistingDestination) {
    auto out = jt::copy(doc(R"({"a":1,"b":2})"), "/a", "/b", jt::DestMode::Replace);
    EXPECT_EQ(out.to_json(), R"({"a":1,"b":1})");
}

TEST(JtCopy, ReplaceNeverCreatesANewDestination) {
    auto out = jt::copy(doc(R"({"a":1})"), "/a", "/b", jt::DestMode::Replace);
    EXPECT_EQ(out.to_json(), R"({"a":1})");
}

TEST(JtCopy, CopiesWholeSubtrees) {
    auto out = jt::copy(doc(R"({"a":{"deep":[1,2]}})"), "/a", "/b");
    EXPECT_EQ(out.to_json(), R"({"a":{"deep":[1,2]},"b":{"deep":[1,2]}})");
}

TEST(JtCopy, MissingSourceIsAnError) {
    EXPECT_THROW(jt::copy(doc(R"({"a":1})"), "/nope", "/b"), jt::Error);
}

TEST(JtCopy, MissingSourceSuggestsANearbyKey) {
    try {
        jt::copy(doc(R"({"user":1})"), "/usr", "/b");
        FAIL() << "expected jt::Error";
    } catch (const jt::Error& e) {
        EXPECT_NE(e.suggestion().find("/user"), std::string::npos);
    }
}

TEST(JtCopy, MissingDestinationIntermediateIsAnError) {
    EXPECT_THROW(jt::copy(doc(R"({"a":1})"), "/a", "/x/y"), jt::Error);
}

TEST(JtCopy, CopyingIntoItsOwnChildIsAllowed) {
    auto out = jt::copy(doc(R"({"a":{"n":1}})"), "/a", "/a/self");
    EXPECT_EQ(out.to_json(), R"({"a":{"n":1,"self":{"n":1}}})");
}

TEST(JtCopy, CopyToAnOutOfRangeArrayIndexIsAnError) {
    // copy() lands through set(), so it must inherit the array range guard
    // instead of null-padding the destination.
    EXPECT_THROW(jt::copy(doc(R"({"a":1,"dst":[1,2]})"), "/a", "/dst/5"), jt::Error);
}

TEST(JtCopy, AppendSentinelWorksAsADestination) {
    auto out = jt::copy(doc(R"({"a":1,"list":[]})"), "/a", "/list/-");
    EXPECT_EQ(out.to_json(), R"({"a":1,"list":[1]})");
}

} // namespace
