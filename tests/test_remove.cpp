// jtRemove — delete the value at a pointer.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/remove.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) { return jsom::parse_document(text); }

TEST(JtRemove, DeletesAnObjectKey) {
    auto out = jt::remove(doc(R"({"a":1,"b":2})"), "/b");
    EXPECT_EQ(out.to_json(), R"({"a":1})");
}

TEST(JtRemove, DeletesANestedKey) {
    auto out = jt::remove(doc(R"({"a":{"x":1,"y":2}})"), "/a/y");
    EXPECT_EQ(out.to_json(), R"({"a":{"x":1}})");
}

TEST(JtRemove, DeletesAWholeSubtree) {
    auto out = jt::remove(doc(R"({"a":{"deep":[1,2]},"b":2})"), "/a");
    EXPECT_EQ(out.to_json(), R"({"b":2})");
}

TEST(JtRemove, DeletesAnArrayElementAndClosesTheGap) {
    auto out = jt::remove(doc(R"({"list":[1,2,3]})"), "/list/1");
    EXPECT_EQ(out.to_json(), R"({"list":[1,3]})");
}

TEST(JtRemove, MissingPathIsAnError) {
    EXPECT_THROW(jt::remove(doc(R"({"a":1})"), "/nope"), jt::Error);
}

TEST(JtRemove, MissingPathSuggestsANearbyKey) {
    try {
        jt::remove(doc(R"({"user":1})"), "/usr");
        FAIL() << "expected jt::Error";
    } catch (const jt::Error& e) {
        EXPECT_NE(e.suggestion().find("/user"), std::string::npos);
    }
}

TEST(JtRemove, MissingIntermediateIsAnError) {
    EXPECT_THROW(jt::remove(doc(R"({"a":1})"), "/x/y"), jt::Error);
}

TEST(JtRemove, OutOfRangeArrayIndexIsAnError) {
    EXPECT_THROW(jt::remove(doc(R"({"list":[1,2]})"), "/list/5"), jt::Error);
}

TEST(JtRemove, RemovingTheRootIsAnError) {
    EXPECT_THROW(jt::remove(doc(R"({"a":1})"), "/"), jt::Error);
}

TEST(JtRemove, RelativePathIsAnError) {
    EXPECT_THROW(jt::remove(doc(R"({"a":1})"), "a"), jt::Error);
}

} // namespace
