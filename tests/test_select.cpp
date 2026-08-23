// jtSelect — pick several paths into a fresh object, flattened to leaf keys.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/select.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtSelect, PicksOnePathUnderItsLeafKey) {
  auto out = jt::select(doc(R"({"user":{"name":"harri"}})"), {"/user/name"});
  EXPECT_EQ(out.to_json(), R"({"name":"harri"})");
}

TEST(JtSelect, FlattensSeveralPaths) {
  auto out = jt::select(doc(R"({"user":{"name":"harri","age":7},"junk":1})"),
                        {"/user/name", "/user/age"});
  EXPECT_EQ(out.to_json(), R"({"age":7,"name":"harri"})");
}

TEST(JtSelect, KeepsWholeSubtrees) {
  auto out = jt::select(doc(R"({"a":{"deep":[1,2]}})"), {"/a"});
  EXPECT_EQ(out.to_json(), R"({"a":{"deep":[1,2]}})");
}

TEST(JtSelect, ArrayIndexBecomesItsOwnLeafKey) {
  auto out = jt::select(doc(R"({"list":["x","y"]})"), {"/list/0"});
  EXPECT_EQ(out.to_json(), R"({"0":"x"})");
}

TEST(JtSelect, DuplicateLeafKeysAreAnError) {
  EXPECT_THROW(jt::select(doc(R"({"a":{"name":1},"b":{"name":2}})"),
                          {"/a/name", "/b/name"}),
               jt::Error);
}

TEST(JtSelect, ZeroPathsIsAnError) {
  EXPECT_THROW(jt::select(doc(R"({"a":1})"), {}), jt::Error);
}

TEST(JtSelect, MissingPathIsAnError) {
  EXPECT_THROW(jt::select(doc(R"({"a":1})"), {"/nope"}), jt::Error);
}

TEST(JtSelect, MissingPathSuggestsANearbyKey) {
  try {
    jt::select(doc(R"({"user":1})"), {"/usr"});
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.suggestion().find("/user"), std::string::npos);
  }
}

TEST(JtSelect, SelectingTheRootIsAnError) {
  EXPECT_THROW(jt::select(doc(R"({"a":1})"), {"/"}), jt::Error);
}

TEST(JtSelect, RelativePathIsAnError) {
  EXPECT_THROW(jt::select(doc(R"({"a":1})"), {"a"}), jt::Error);
}

}  // namespace
