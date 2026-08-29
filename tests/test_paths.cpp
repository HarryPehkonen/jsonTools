// jt::paths — pointer and parent-container contracts shared by every verb.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/paths.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtPaths, RequireParentPassesAnObjectParent) {
  EXPECT_NO_THROW(jt::require_parent(doc(R"({"a":{}})"), "/a/b", "hint"));
}

TEST(JtPaths, RequireParentPassesTheRootPointer) {
  EXPECT_NO_THROW(jt::require_parent(doc(R"({"a":1})"), "", "hint"));
}

TEST(JtPaths, RequireParentPassesAnInRangeArrayIndex) {
  EXPECT_NO_THROW(jt::require_parent(doc(R"({"a":[1,2]})"), "/a/1", "hint"));
}

TEST(JtPaths, RequireParentPassesTheAppendSentinel) {
  EXPECT_NO_THROW(jt::require_parent(doc(R"({"a":[1]})"), "/a/-", "hint"));
}

TEST(JtPaths, RequireParentStillReportsAMissingParent) {
  try {
    jt::require_parent(doc(R"({"a":{}})"), "/a/b/c", "create it first");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_EQ(e.path(), "/a/b");
    EXPECT_NE(e.problem().find("missing intermediate path"), std::string::npos);
    EXPECT_NE(e.suggestion().find("create it first"), std::string::npos);
  }
}

TEST(JtPaths, RequireParentRejectsAScalarParent) {
  // The header promises a container check, not a mere existence check: a
  // string parent can never hold a new key.
  try {
    jt::require_parent(doc(R"({"a":"s"})"), "/a/b", "hint");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_EQ(e.path(), "/a");
    EXPECT_NE(e.problem().find("inside a string"), std::string::npos);
    EXPECT_NE(e.suggestion().find("replace"), std::string::npos);
  }
}

TEST(JtPaths, RequireParentRejectsAnOutOfRangeArrayIndex) {
  // Without the range check, set() null-pads the array up to the index.
  try {
    jt::require_parent(doc(R"({"a":[1,2]})"), "/a/5", "hint");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_EQ(e.path(), "/a/5");
    EXPECT_NE(e.problem().find("out of range"), std::string::npos);
    EXPECT_NE(e.suggestion().find("2 elements"), std::string::npos);
  }
}

TEST(JtPaths, RequireParentRejectsANonIndexLeafOnAnArray) {
  try {
    jt::require_parent(doc(R"({"a":[1,2]})"), "/a/name", "hint");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_EQ(e.path(), "/a/name");
    EXPECT_NE(e.problem().find("not an array index"), std::string::npos);
  }
}

}  // namespace
