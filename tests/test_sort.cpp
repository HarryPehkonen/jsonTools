// jtSort — stable multi-key sort of a list.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/sort.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtSort, SortsTheWholeDocumentWhenItIsTheList) {
  auto out = jt::sort(doc("[3,1,2]"), "/", {});
  EXPECT_EQ(out.to_json(), "[1,2,3]");
}

TEST(JtSort, SortsNumbersNumericallyNotLexically) {
  auto out = jt::sort(doc("[10,9,100]"), "/", {});
  EXPECT_EQ(out.to_json(), "[9,10,100]");
}

TEST(JtSort, SortsStringsLexically) {
  auto out = jt::sort(doc(R"(["pear","apple","fig"])"), "/", {});
  EXPECT_EQ(out.to_json(), R"(["apple","fig","pear"])");
}

TEST(JtSort, MixedTypesWithoutKeysIsAnError) {
  EXPECT_THROW(jt::sort(doc(R"([1,"a"])"), "/", {}), jt::Error);
}

TEST(JtSort, ObjectsWithoutKeysAreAnError) {
  EXPECT_THROW(jt::sort(doc(R"([{"a":1},{"a":2}])"), "/", {}), jt::Error);
}

TEST(JtSort, SortsANestedListByKey) {
  auto out = jt::sort(doc(R"({"items":[{"n":"b"},{"n":"a"}]})"), "/items",
                      {{"n", false}});
  EXPECT_EQ(out.to_json(), R"({"items":[{"n":"a"},{"n":"b"}]})");
}

TEST(JtSort, DescendingReversesASingleKey) {
  auto out = jt::sort(doc(R"([{"n":1},{"n":3},{"n":2}])"), "/", {{"n", true}});
  EXPECT_EQ(out.to_json(), R"([{"n":3},{"n":2},{"n":1}])");
}

TEST(JtSort, SortsByASecondKeyWhenTheFirstTies) {
  auto out = jt::sort(doc(R"([{"a":1,"b":2},{"a":1,"b":1},{"a":0,"b":9}])"), "/",
                      {{"a", false}, {"b", false}});
  EXPECT_EQ(out.to_json(), R"([{"a":0,"b":9},{"a":1,"b":1},{"a":1,"b":2}])");
}

TEST(JtSort, MixesDirectionsAcrossKeys) {
  auto out = jt::sort(doc(R"([{"a":1,"b":1},{"a":1,"b":2},{"a":0,"b":0}])"), "/",
                      {{"a", false}, {"b", true}});
  EXPECT_EQ(out.to_json(), R"([{"a":0,"b":0},{"a":1,"b":2},{"a":1,"b":1}])");
}

TEST(JtSort, IsStableForEqualKeys) {
  auto out = jt::sort(doc(R"([{"a":1,"i":0},{"a":1,"i":1},{"a":1,"i":2}])"), "/",
                      {{"a", false}});
  EXPECT_EQ(out.to_json(), R"([{"a":1,"i":0},{"a":1,"i":1},{"a":1,"i":2}])");
}

TEST(JtSort, ReachesNestedKeysWithARelativePath) {
  auto out = jt::sort(doc(R"([{"n":{"last":"b"}},{"n":{"last":"a"}}])"), "/",
                      {{"n/last", false}});
  EXPECT_EQ(out.to_json(), R"([{"n":{"last":"a"}},{"n":{"last":"b"}}])");
}

TEST(JtSort, AnElementMissingTheKeyIsAnError) {
  EXPECT_THROW(jt::sort(doc(R"([{"n":1},{"m":2}])"), "/", {{"n", false}}),
               jt::Error);
}

TEST(JtSort, MixedTypesUnderAKeyIsAnError) {
  EXPECT_THROW(jt::sort(doc(R"([{"n":1},{"n":"a"}])"), "/", {{"n", false}}),
               jt::Error);
}

TEST(JtSort, NonObjectElementsWithKeysAreAnError) {
  EXPECT_THROW(jt::sort(doc("[1,2]"), "/", {{"n", false}}), jt::Error);
}

TEST(JtSort, AKeyWithALeadingSlashIsAnError) {
  EXPECT_THROW(jt::sort(doc(R"([{"n":1}])"), "/", {{"/n", false}}), jt::Error);
}

TEST(JtSort, ElementErrorsPointAtAPastableElementPath) {
  try {
    jt::sort(doc(R"([1,"a"])"), "/", {});
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_EQ(e.path(), "/1");
  }
}

TEST(JtSort, TheListPathMustBeAnArray) {
  EXPECT_THROW(jt::sort(doc(R"({"items":{"a":1}})"), "/items", {}), jt::Error);
}

TEST(JtSort, MissingListPathIsAnError) {
  EXPECT_THROW(jt::sort(doc(R"({"items":[]})"), "/nope", {}), jt::Error);
}

}  // namespace
