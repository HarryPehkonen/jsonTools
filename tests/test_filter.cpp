// jtFilter — keep the list elements whose key satisfies a comparison.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/filter.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) { return jsom::parse_document(text); }

TEST(JtFilter, KeepsNumericMatchesGreaterThan) {
    auto out = jt::filter(doc(R"([{"age":20},{"age":10}])"), "/", "age", jt::Op::Gt, doc("18"));
    EXPECT_EQ(out.to_json(), R"([{"age":20}])");
}

TEST(JtFilter, ComparesStringsForEquality) {
    auto out
        = jt::filter(doc(R"([{"n":"bob"},{"n":"ann"}])"), "/", "n", jt::Op::Eq, doc(R"("bob")"));
    EXPECT_EQ(out.to_json(), R"([{"n":"bob"}])");
}

TEST(JtFilter, FiltersANestedList) {
    auto out
        = jt::filter(doc(R"({"items":[{"a":1},{"a":2}]})"), "/items", "a", jt::Op::Le, doc("1"));
    EXPECT_EQ(out.to_json(), R"({"items":[{"a":1}]})");
}

TEST(JtFilter, SupportsEveryOperator) {
    const auto list = doc(R"([{"a":1},{"a":2},{"a":3}])");
    EXPECT_EQ(jt::filter(list, "/", "a", jt::Op::Eq, doc("2")).to_json(), R"([{"a":2}])");
    EXPECT_EQ(jt::filter(list, "/", "a", jt::Op::Ne, doc("2")).to_json(), R"([{"a":1},{"a":3}])");
    EXPECT_EQ(jt::filter(list, "/", "a", jt::Op::Gt, doc("2")).to_json(), R"([{"a":3}])");
    EXPECT_EQ(jt::filter(list, "/", "a", jt::Op::Ge, doc("2")).to_json(), R"([{"a":2},{"a":3}])");
    EXPECT_EQ(jt::filter(list, "/", "a", jt::Op::Lt, doc("2")).to_json(), R"([{"a":1}])");
    EXPECT_EQ(jt::filter(list, "/", "a", jt::Op::Le, doc("2")).to_json(), R"([{"a":1},{"a":2}])");
}

TEST(JtFilter, KeepsTheOriginalOrder) {
    auto out = jt::filter(doc(R"([{"a":3},{"a":1},{"a":2}])"), "/", "a", jt::Op::Ge, doc("2"));
    EXPECT_EQ(out.to_json(), R"([{"a":3},{"a":2}])");
}

TEST(JtFilter, ReachesNestedKeysWithARelativePath) {
    auto out = jt::filter(doc(R"([{"n":{"last":"a"}},{"n":{"last":"b"}}])"), "/", "n/last",
                          jt::Op::Eq, doc(R"("b")"));
    EXPECT_EQ(out.to_json(), R"([{"n":{"last":"b"}}])");
}

TEST(JtFilter, AnElementMissingTheKeyIsDroppedSilently) {
    auto out = jt::filter(doc(R"([{"a":1},{"b":2}])"), "/", "a", jt::Op::Eq, doc("1"));
    EXPECT_EQ(out.to_json(), R"([{"a":1}])");
}

TEST(JtFilter, AScalarElementIsDroppedSilently) {
    auto out = jt::filter(doc(R"([{"a":1},7])"), "/", "a", jt::Op::Eq, doc("1"));
    EXPECT_EQ(out.to_json(), R"([{"a":1}])");
}

TEST(JtFilter, EqualityNeverCoercesAcrossTypes) {
    auto out = jt::filter(doc(R"([{"a":1},{"a":"1"}])"), "/", "a", jt::Op::Eq, doc("1"));
    EXPECT_EQ(out.to_json(), R"([{"a":1}])");
}

TEST(JtFilter, OrderingAcrossTypesIsAnError) {
    EXPECT_THROW(jt::filter(doc(R"([{"a":"x"}])"), "/", "a", jt::Op::Gt, doc("1")), jt::Error);
}

TEST(JtFilter, OrderingAgainstANonScalarLiteralIsAnError) {
    EXPECT_THROW(jt::filter(doc(R"([{"a":1}])"), "/", "a", jt::Op::Gt, doc("[]")), jt::Error);
}

TEST(JtFilter, ElementErrorsPointAtAPastableElementPath) {
    try {
        jt::filter(doc(R"([{"a":"x"}])"), "/", "a", jt::Op::Gt, doc("1"));
        FAIL() << "expected jt::Error";
    } catch (const jt::Error& e) {
        EXPECT_EQ(e.path(), "/0/a");
    }
}

TEST(JtFilter, AKeyWithALeadingSlashIsAnError) {
    EXPECT_THROW(jt::filter(doc(R"([{"a":1}])"), "/", "/a", jt::Op::Eq, doc("1")), jt::Error);
}

TEST(JtFilter, TheListPathMustBeAnArray) {
    EXPECT_THROW(jt::filter(doc(R"({"a":1})"), "/", "a", jt::Op::Eq, doc("1")), jt::Error);
}

TEST(JtFilter, MissingListPathIsAnError) {
    EXPECT_THROW(jt::filter(doc(R"({"items":[]})"), "/nope", "a", jt::Op::Eq, doc("1")), jt::Error);
}

TEST(JtFilter, OutputIsNeverLongerThanInput) {
    auto out = jt::filter(doc(R"([{"a":1},{"a":2},{"a":3}])"), "/", "a", jt::Op::Ge, doc("0"));
    EXPECT_EQ(out.size(), 3u);
}

TEST(JtFilter, OperatorFlagsMapToOperators) {
    jt::Op op = jt::Op::Eq;
    EXPECT_TRUE(jt::op_from_flag("--gt", op));
    EXPECT_EQ(op, jt::Op::Gt);
    EXPECT_FALSE(jt::op_from_flag("--nope", op));
}

} // namespace
