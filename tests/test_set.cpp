// jtSet — set a literal at a pointer, mkdir -p style only with -p.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/set.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtSet, SetsAKeyOnAnObject) {
  auto out = jt::set(doc("{}"), "/name", doc(R"("harri")"));
  EXPECT_EQ(out.to_json(), R"({"name":"harri"})");
}

TEST(JtSet, OverwritesAnExistingKey) {
  auto out = jt::set(doc(R"({"n":1})"), "/n", doc("2"));
  EXPECT_EQ(out.to_json(), R"({"n":2})");
}

TEST(JtSet, LiteralTypesAreNotCoerced) {
  EXPECT_EQ(jt::set(doc("{}"), "/x", doc("18")).to_json(), R"({"x":18})");
  EXPECT_EQ(jt::set(doc("{}"), "/x", doc(R"("18")")).to_json(), R"({"x":"18"})");
  EXPECT_EQ(jt::set(doc("{}"), "/x", doc("null")).to_json(), R"({"x":null})");
  EXPECT_EQ(jt::set(doc("{}"), "/x", doc("true")).to_json(), R"({"x":true})");
  EXPECT_EQ(jt::set(doc("{}"), "/x", doc(R"([1,2])")).to_json(), R"({"x":[1,2]})");
}

TEST(JtSet, CreatesTheLeafButNotIntermediates) {
  EXPECT_THROW(jt::set(doc("{}"), "/a/b", doc("1")), jt::Error);
}

TEST(JtSet, MkdirPCreatesIntermediateObjects) {
  auto out = jt::set(doc("{}"), "/a/b/c", doc("1"), /*mkdir_p=*/true);
  EXPECT_EQ(out.to_json(), R"({"a":{"b":{"c":1}}})");
}

TEST(JtSet, MkdirPKeepsExistingSiblings) {
  auto out = jt::set(doc(R"({"a":{"keep":true}})"), "/a/b", doc("1"), true);
  EXPECT_EQ(out.to_json(), R"({"a":{"b":1,"keep":true}})");
}

TEST(JtSet, MkdirPNeverCreatesArrays) {
  // A numeric segment is still just an object key when -p has to invent the
  // container: -p creates objects only.
  auto out = jt::set(doc("{}"), "/a/0/b", doc("1"), true);
  EXPECT_EQ(out.to_json(), R"({"a":{"0":{"b":1}}})");
}

TEST(JtSet, MkdirPDoesNotGrowAnExistingArray) {
  EXPECT_THROW(jt::set(doc(R"({"a":[]})"), "/a/0/b", doc("1"), true), jt::Error);
}

TEST(JtSet, MkdirPWillNotTunnelThroughAScalar) {
  EXPECT_THROW(jt::set(doc(R"({"a":5})"), "/a/b", doc("1"), true), jt::Error);
}

TEST(JtSet, MkdirPSetsIntoAnExistingArrayElement) {
  // -p used to refuse arrays outright, breaking the common "add a field to
  // element 0" case.
  auto out = jt::set(doc(R"({"items":[{"sub":1}]})"), "/items/0/x", doc("1"), true);
  EXPECT_EQ(out.to_json(), R"({"items":[{"sub":1,"x":1}]})");
}

TEST(JtSet, MkdirPSetsDeeplyIntoAnExistingArrayElement) {
  auto out = jt::set(doc(R"({"items":[{"sub":1}]})"), "/items/0/x/y", doc("1"), true);
  EXPECT_EQ(out.to_json(), R"({"items":[{"sub":1,"x":{"y":1}}]})");
}

TEST(JtSet, MkdirPAppendHintActuallyUnlocksTheNextIndex) {
  auto d = doc(R"({"items":[1,2]})");
  try {
    d = jt::set(d, "/items/2/x", doc("1"), true);
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.problem().find("out of range"), std::string::npos);
    EXPECT_NE(e.suggestion().find("jtSet /items/-"), std::string::npos);
  }
  d = jt::set(d, "/items/-", doc("{}")); // follow the hint
  d = jt::set(d, "/items/2/x", doc("1"), true); // the original command now works
  EXPECT_EQ(d.to_json(), R"({"items":[1,2,{"x":1}]})");
}

TEST(JtSet, MkdirPDoesNotGrowAnArrayAtTheLeafEither) {
  try {
    jt::set(doc(R"({"items":[1,2]})"), "/items/5", doc("9"), true);
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.problem().find("out of range"), std::string::npos);
  }
}

TEST(JtSet, SetsAnArrayElementByIndex) {
  auto out = jt::set(doc(R"({"a":[1,2,3]})"), "/a/1", doc("9"));
  EXPECT_EQ(out.to_json(), R"({"a":[1,9,3]})");
}

TEST(JtSet, SetToAnOutOfRangeArrayIndexIsAnError) {
  // Without this guard, set() null-pads the array up to the index:
  // {"a":[1,2,null,null,null,9]}.
  try {
    jt::set(doc(R"({"a":[1,2]})"), "/a/5", doc("9"));
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.problem().find("out of range"), std::string::npos);
    EXPECT_NE(e.suggestion().find("2 elements"), std::string::npos);
  }
}

TEST(JtSet, SetWithANonIndexSegmentOnAnArrayIsAnError) {
  try {
    jt::set(doc(R"({"a":[1,2]})"), "/a/name", doc("9"));
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.problem().find("not an array index"), std::string::npos);
  }
}

TEST(JtSet, AppendSentinelAppends) {
  auto out = jt::set(doc(R"({"items":["a"]})"), "/items/-", doc(R"("b")"));
  EXPECT_EQ(out.to_json(), R"({"items":["a","b"]})");
}

TEST(JtSet, AppendSentinelOnANonArrayIsAnError) {
  EXPECT_THROW(jt::set(doc(R"({"items":{}})"), "/items/-", doc("1")), jt::Error);
}

TEST(JtSet, SettingRootReplacesTheDocument) {
  EXPECT_EQ(jt::set(doc(R"({"a":1})"), "/", doc("[1,2]")).to_json(), "[1,2]");
  EXPECT_EQ(jt::set(doc(R"({"a":1})"), "", doc("[1,2]")).to_json(), "[1,2]");
}

TEST(JtSet, RelativePathsAreRejected) {
  EXPECT_THROW(jt::set(doc("{}"), "name", doc("1")), jt::Error);
}

TEST(JtSet, SettingAKeyOnAScalarIsAnError) {
  EXPECT_THROW(jt::set(doc(R"({"a":5})"), "/a/b", doc("1")), jt::Error);
}

TEST(JtSet, EscapedPointerSegmentsRoundTrip) {
  auto out = jt::set(doc("{}"), "/a~1b", doc("1"));
  EXPECT_EQ(out.to_json(), R"({"a/b":1})");
}

TEST(JtSet, MissingParentErrorSuggestsANearbyKey) {
  try {
    jt::set(doc(R"({"user":{"name":"x"}})"), "/usr/name", doc("1"));
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.suggestion().find("user"), std::string::npos);
  }
}

}  // namespace
