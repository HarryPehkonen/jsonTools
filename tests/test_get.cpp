// jtGet — the document becomes the value at a pointer.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/get.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtGet, ExtractsAScalar) {
  auto out = jt::get(doc(R"({"a":1})"), "/a");
  EXPECT_EQ(out.to_json(), "1");
}

TEST(JtGet, ExtractsANestedSubtree) {
  auto out = jt::get(doc(R"({"a":{"b":[1,2]}})"), "/a/b");
  EXPECT_EQ(out.to_json(), "[1,2]");
}

TEST(JtGet, ExtractsAnArrayElement) {
  auto out = jt::get(doc(R"({"list":["x","y"]})"), "/list/1");
  EXPECT_EQ(out.to_json(), R"("y")");
}

TEST(JtGet, RootIsTheIdentity) {
  auto out = jt::get(doc(R"({"a":1})"), "/");
  EXPECT_EQ(out.to_json(), R"({"a":1})");
}

TEST(JtGet, MissingPathIsAnError) {
  EXPECT_THROW(jt::get(doc(R"({"a":1})"), "/nope"), jt::Error);
}

TEST(JtGet, MissingPathSuggestsANearbyKey) {
  try {
    jt::get(doc(R"({"user":1})"), "/usr");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.suggestion().find("/user"), std::string::npos);
  }
}

TEST(JtGet, DefaultSuppliesTheValueWhenThePathIsMissing) {
  jsom::JsonDocument fallback = doc(R"("none")");
  auto out = jt::get(doc(R"({"a":1})"), "/nope", &fallback);
  EXPECT_EQ(out.to_json(), R"("none")");
}

TEST(JtGet, DefaultIsIgnoredWhenThePathExists) {
  jsom::JsonDocument fallback = doc(R"("none")");
  auto out = jt::get(doc(R"({"a":1})"), "/a", &fallback);
  EXPECT_EQ(out.to_json(), "1");
}

TEST(JtGet, DefaultCoversAMissingIntermediate) {
  jsom::JsonDocument fallback = doc("[]");
  auto out = jt::get(doc(R"({"a":1})"), "/x/y", &fallback);
  EXPECT_EQ(out.to_json(), "[]");
}

TEST(JtGet, AnExistingNullIsNotMissing) {
  jsom::JsonDocument fallback = doc("42");
  auto out = jt::get(doc(R"({"a":null})"), "/a", &fallback);
  EXPECT_EQ(out.to_json(), "null");
}

TEST(JtGet, RelativePathIsAnError) {
  EXPECT_THROW(jt::get(doc(R"({"a":1})"), "a"), jt::Error);
}

}  // namespace
