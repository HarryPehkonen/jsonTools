// jtZip — build an object from a keys list and a values list.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/zip.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) { return jsom::parse_document(text); }

TEST(JtZip, PairsKeysWithValues) {
    auto out = jt::zip(doc(R"({"k":["a","b"],"v":[1,2]})"), "/k", "/v");
    EXPECT_EQ(out.to_json(), R"({"a":1,"b":2})");
}

TEST(JtZip, KeepsWholeSubtreesAsValues) {
    auto out = jt::zip(doc(R"({"k":["a"],"v":[{"deep":[1]}]})"), "/k", "/v");
    EXPECT_EQ(out.to_json(), R"({"a":{"deep":[1]}})");
}

TEST(JtZip, EmptyListsMakeAnEmptyObject) {
    auto out = jt::zip(doc(R"({"k":[],"v":[]})"), "/k", "/v");
    EXPECT_EQ(out.to_json(), "{}");
}

TEST(JtZip, LengthMismatchIsAnError) {
    EXPECT_THROW(jt::zip(doc(R"({"k":["a","b"],"v":[1]})"), "/k", "/v"), jt::Error);
}

TEST(JtZip, CollidingKeysAreAnError) {
    EXPECT_THROW(jt::zip(doc(R"({"k":["a","a"],"v":[1,2]})"), "/k", "/v"), jt::Error);
}

TEST(JtZip, OverwriteMakesCollisionsLastWins) {
    auto out = jt::zip(doc(R"({"k":["a","a"],"v":[1,2]})"), "/k", "/v", true);
    EXPECT_EQ(out.to_json(), R"({"a":2})");
}

TEST(JtZip, NonStringKeysAreAnError) {
    EXPECT_THROW(jt::zip(doc(R"({"k":[1],"v":[2]})"), "/k", "/v"), jt::Error);
}

TEST(JtZip, KeysPathMustBeAnArray) {
    EXPECT_THROW(jt::zip(doc(R"({"k":{"a":1},"v":[1]})"), "/k", "/v"), jt::Error);
}

TEST(JtZip, ValuesPathMustBeAnArray) {
    EXPECT_THROW(jt::zip(doc(R"({"k":["a"],"v":7})"), "/k", "/v"), jt::Error);
}

TEST(JtZip, MissingKeysPathIsAnError) {
    EXPECT_THROW(jt::zip(doc(R"({"v":[1]})"), "/nope", "/v"), jt::Error);
}

TEST(JtZip, MissingPathSuggestsANearbyKey) {
    try {
        jt::zip(doc(R"({"keys":["a"],"v":[1]})"), "/kets", "/v");
        FAIL() << "expected jt::Error";
    } catch (const jt::Error& e) {
        EXPECT_NE(e.suggestion().find("/keys"), std::string::npos);
    }
}

} // namespace
