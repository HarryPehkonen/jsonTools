// jtMove — copy then delete the source, subject to the destination guard.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/move.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtMove, RenamesAKey) {
  auto out = jt::move(doc(R"({"oldKey":1})"), "/oldKey", "/newKey");
  EXPECT_EQ(out.to_json(), R"({"newKey":1})");
}

TEST(JtMove, MovesAcrossContainers) {
  auto out = jt::move(doc(R"({"a":{"n":1},"b":{}})"), "/a/n", "/b/n");
  EXPECT_EQ(out.to_json(), R"({"a":{},"b":{"n":1}})");
}

TEST(JtMove, DefaultOverwritesTheDestination) {
  auto out = jt::move(doc(R"({"a":1,"b":2})"), "/a", "/b");
  EXPECT_EQ(out.to_json(), R"({"b":1})");
}

TEST(JtMove, IfNotSetBlockedMoveLeavesTheSourceInPlace) {
  auto out = jt::move(doc(R"({"a":1,"b":2})"), "/a", "/b", jt::DestMode::IfNotSet);
  EXPECT_EQ(out.to_json(), R"({"a":1,"b":2})");
}

TEST(JtMove, IfNotSetWritesAnAbsentDestination) {
  auto out = jt::move(doc(R"({"a":1})"), "/a", "/b", jt::DestMode::IfNotSet);
  EXPECT_EQ(out.to_json(), R"({"b":1})");
}

TEST(JtMove, ReplaceBlockedMoveLeavesTheSourceInPlace) {
  auto out = jt::move(doc(R"({"a":1})"), "/a", "/b", jt::DestMode::Replace);
  EXPECT_EQ(out.to_json(), R"({"a":1})");
}

TEST(JtMove, ReplaceOverwritesAnExistingDestination) {
  auto out = jt::move(doc(R"({"a":1,"b":2})"), "/a", "/b", jt::DestMode::Replace);
  EXPECT_EQ(out.to_json(), R"({"b":1})");
}

TEST(JtMove, MissingSourceIsAnError) {
  EXPECT_THROW(jt::move(doc(R"({"a":1})"), "/nope", "/b"), jt::Error);
}

TEST(JtMove, MovingIntoItsOwnChildIsAnError) {
  EXPECT_THROW(jt::move(doc(R"({"a":{"n":1}})"), "/a", "/a/self"), jt::Error);
}

TEST(JtMove, MovingTheDocumentRootIsAnError) {
  EXPECT_THROW(jt::move(doc(R"({"a":1})"), "/", "/b"), jt::Error);
}

TEST(JtMove, MovingOntoItselfIsAnIdentity) {
  auto out = jt::move(doc(R"({"a":1})"), "/a", "/a");
  EXPECT_EQ(out.to_json(), R"({"a":1})");
}

// Property: a move and its inverse restore the original document.
TEST(JtMove, MoveThenMoveBackRoundTrips) {
  const std::string original = R"({"x":{"deep":[1,2]},"y":9})";
  auto there = jt::move(doc(original), "/x", "/z");
  auto back = jt::move(std::move(there), "/z", "/x");
  EXPECT_EQ(back.to_json(), original);
}

TEST(JtMove, MissingDestinationIntermediateIsAnError) {
  EXPECT_THROW(jt::move(doc(R"({"a":1})"), "/a", "/x/y"), jt::Error);
}

TEST(JtMove, MoveToAnOutOfRangeArrayIndexIsAnError) {
  // The destination guard must run before the write: without it, move()
  // null-pads the destination array.
  EXPECT_THROW(jt::move(doc(R"({"src":1,"dst":[1,2]})"), "/src", "/dst/5"), jt::Error);
}

TEST(JtMove, MoveOntoAScalarParentNamesTheOffendingValue) {
  try {
    jt::move(doc(R"({"src":1,"a":5})"), "/src", "/a/b");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_EQ(e.path(), "/a");
    EXPECT_NE(e.problem().find("inside a number"), std::string::npos);
  }
}

}  // namespace
