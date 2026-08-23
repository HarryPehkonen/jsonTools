// jtLen — write a container's length into the document, leaving it intact.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/len.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtLen, WritesAListLengthAlongsideTheList) {
  auto out = jt::len(doc(R"({"items":[1,2,3]})"), "/items", "/count");
  EXPECT_EQ(out.to_json(), R"({"count":3,"items":[1,2,3]})");
}

TEST(JtLen, CountsObjectKeys) {
  auto out = jt::len(doc(R"({"o":{"a":1,"b":2}})"), "/o", "/n");
  EXPECT_EQ(out.to_json(), R"({"n":2,"o":{"a":1,"b":2}})");
}

TEST(JtLen, AnEmptyListIsZero) {
  auto out = jt::len(doc(R"({"items":[]})"), "/items", "/count");
  EXPECT_EQ(out.to_json(), R"({"count":0,"items":[]})");
}

TEST(JtLen, CreatesTheDestinationLeaf) {
  auto out = jt::len(doc(R"({"a":{"items":[1]}})"), "/a/items", "/a/count");
  EXPECT_EQ(out.to_json(), R"({"a":{"count":1,"items":[1]}})");
}

TEST(JtLen, OverwritesAnExistingDestination) {
  auto out = jt::len(doc(R"({"items":[1,2],"count":99})"), "/items", "/count");
  EXPECT_EQ(out.to_json(), R"({"count":2,"items":[1,2]})");
}

TEST(JtLen, TheWholeDocumentCanBeTheList) {
  auto out = jt::len(doc(R"({"a":1,"b":2})"), "/", "/n");
  EXPECT_EQ(out.to_json(), R"({"a":1,"b":2,"n":2})");
}

TEST(JtLen, MissingDestinationIntermediateIsAnError) {
  EXPECT_THROW(jt::len(doc(R"({"items":[1]})"), "/items", "/x/count"), jt::Error);
}

TEST(JtLen, DashPCreatesTheMissingIntermediate) {
  auto out = jt::len(doc(R"({"items":[1]})"), "/items", "/x/count", true);
  EXPECT_EQ(out.to_json(), R"({"items":[1],"x":{"count":1}})");
}

TEST(JtLen, AScalarHasNoLength) {
  EXPECT_THROW(jt::len(doc(R"({"a":7})"), "/a", "/n"), jt::Error);
}

TEST(JtLen, MissingListPathIsAnError) {
  EXPECT_THROW(jt::len(doc(R"({"items":[1]})"), "/nope", "/n"), jt::Error);
}

TEST(JtLen, MissingListPathSuggestsANearbyKey) {
  try {
    jt::len(doc(R"({"items":[1]})"), "/item", "/n");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.suggestion().find("/items"), std::string::npos);
  }
}

TEST(JtLen, TheRootIsNotAValidDestination) {
  EXPECT_THROW(jt::len(doc(R"({"items":[1]})"), "/items", "/"), jt::Error);
}

}  // namespace
