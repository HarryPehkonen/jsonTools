// jtKeys — the document becomes the key list of an object.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/keys.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtKeys, ListsTheKeysOfTheWholeDocument) {
  EXPECT_EQ(jt::keys(doc(R"({"b":1,"a":2})")).to_json(), R"(["a","b"])");
}

TEST(JtKeys, ListsTheKeysOfANestedObject) {
  EXPECT_EQ(jt::keys(doc(R"({"user":{"name":1,"age":2}})"), "/user").to_json(),
            R"(["age","name"])");
}

TEST(JtKeys, AnEmptyObjectHasNoKeys) {
  EXPECT_EQ(jt::keys(doc("{}")).to_json(), "[]");
}

TEST(JtKeys, AnArrayHasNoKeys) {
  EXPECT_THROW(jt::keys(doc("[1,2]")), jt::Error);
}

TEST(JtKeys, AScalarHasNoKeys) {
  EXPECT_THROW(jt::keys(doc(R"({"a":1})"), "/a"), jt::Error);
}

TEST(JtKeys, MissingPathIsAnError) {
  EXPECT_THROW(jt::keys(doc(R"({"a":1})"), "/nope"), jt::Error);
}

TEST(JtKeys, MissingPathSuggestsANearbyKey) {
  try {
    jt::keys(doc(R"({"user":{}})"), "/usr");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.suggestion().find("/user"), std::string::npos);
  }
}

TEST(JtKeys, RelativePathIsAnError) {
  EXPECT_THROW(jt::keys(doc(R"({"a":1})"), "a"), jt::Error);
}

}  // namespace
