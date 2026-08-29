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

// --- key validation (review issue 5) ---

TEST(JtSort, AnEmptyKeyIsAnErrorEvenOnAValidList) {
  // jtFilter rejects an empty key path; jtSort must not accept one either.
  try {
    jt::sort(doc(R"([{"n":1},{"n":2}])"), "/", {{"", false}});
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.problem().find("empty key path"), std::string::npos);
  }
}

TEST(JtSort, AnEmptyDescendingKeyIsAnErrorToo) {
  // --desc-for "" reaches sort() as a descending SortKey; same validation.
  try {
    jt::sort(doc(R"([{"n":1}])"), "/", {{"n", false}, {"", true}});
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.problem().find("empty key path"), std::string::npos);
  }
}

TEST(JtSort, KeysAreValidatedBeforeTheListIsResolved) {
  // The key is bad regardless of what the list turns out to be, so the
  // empty-key error must win over "cannot sort a number" (jtFilter already
  // validates its key before touching the document).
  try {
    jt::sort(doc(R"({"items":5})"), "/items", {{"", false}});
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.problem().find("empty key path"), std::string::npos);
    EXPECT_EQ(e.problem().find("cannot sort"), std::string::npos);
  }
}

TEST(JtSort, NoKeysSortsTheWholeDocumentByNaturalType) {
  // Bare `jtSort` (no listPath, no keys) is decided behavior, not an
  // accident: it natural-sorts the document itself (REQUIREMENTS §9.2).
  auto out = jt::sort(doc(R"([3,1,2])"), "/", {});
  EXPECT_EQ(out.to_json(), "[1,2,3]");
}

}  // namespace
