// jtType — the document becomes the type name of the value at a path.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/type.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) {
  return jsom::parse_document(text);
}

TEST(JtType, NamesEveryJsonType) {
  const auto sample =
      doc(R"({"o":{},"a":[],"s":"x","n":1,"b":true,"z":null})");
  EXPECT_EQ(jt::type(sample, "/o").to_json(), R"("object")");
  EXPECT_EQ(jt::type(sample, "/a").to_json(), R"("array")");
  EXPECT_EQ(jt::type(sample, "/s").to_json(), R"("string")");
  EXPECT_EQ(jt::type(sample, "/n").to_json(), R"("number")");
  EXPECT_EQ(jt::type(sample, "/b").to_json(), R"("boolean")");
  EXPECT_EQ(jt::type(sample, "/z").to_json(), R"("null")");
}

TEST(JtType, DefaultsToTheWholeDocument) {
  EXPECT_EQ(jt::type(doc(R"({"a":1})")).to_json(), R"("object")");
  EXPECT_EQ(jt::type(doc("[1]")).to_json(), R"("array")");
}

TEST(JtType, ReachesNestedPaths) {
  EXPECT_EQ(jt::type(doc(R"({"a":{"b":[1]}})"), "/a/b/0").to_json(),
            R"("number")");
}

TEST(JtType, MissingPathIsAnError) {
  EXPECT_THROW(jt::type(doc(R"({"a":1})"), "/nope"), jt::Error);
}

TEST(JtType, MissingPathSuggestsANearbyKey) {
  try {
    jt::type(doc(R"({"user":1})"), "/usr");
    FAIL() << "expected jt::Error";
  } catch (const jt::Error& e) {
    EXPECT_NE(e.suggestion().find("/user"), std::string::npos);
  }
}

TEST(JtType, RelativePathIsAnError) {
  EXPECT_THROW(jt::type(doc(R"({"a":1})"), "a"), jt::Error);
}

}  // namespace
