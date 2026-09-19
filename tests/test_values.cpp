// jtKeys/jtValues — reduction + Form B (composable) round-trip.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/keys.hpp"
#include "jt/values.hpp"

namespace {

jsom::JsonDocument doc(const std::string& text) { return jsom::parse_document(text); }

// --- jtKeys Form B ---

TEST(JtKeys, FormBWritesKeysIntoTheDocument) {
    const std::string in = R"({"b":1,"a":2})";
    const std::string out = R"({"a":2,"b":1,"keys":["a","b"]})";
    EXPECT_EQ(jt::keys_to(doc(in), "/", "/keys").to_json(), out);
}

TEST(JtKeys, FormBOnNestedObject) {
    const std::string in = R"({"u":{"x":1,"y":2}})";
    const std::string out = R"({"k":["x","y"],"u":{"x":1,"y":2}})";
    EXPECT_EQ(jt::keys_to(doc(in), "/u", "/k").to_json(), out);
}

TEST(JtKeys, FormBAtRootIsAnError) {
    EXPECT_THROW(jt::keys_to(doc(R"({"a":1})"), "/", "/"), jt::Error);
}

// --- jtValues reduction ---

TEST(JtValues, ListsTheValuesOfTheWholeDocument) {
    EXPECT_EQ(jt::values_array(doc(R"({"b":1,"a":2})")).to_json(), R"([2,1])");
}

TEST(JtValues, ListsTheValuesOfANestedObject) {
    EXPECT_EQ(jt::values_array(doc(R"({"u":{"x":10,"y":20}})"), "/u").to_json(), R"([10,20])");
}

TEST(JtValues, AnArrayHasNoValuesInTheObjectSense) {
    EXPECT_THROW(jt::values_array(doc("[1,2]")), jt::Error);
}

// --- jtValues Form B ---

TEST(JtValues, FormBWritesValuesIntoTheDocument) {
    const std::string in = R"({"b":1,"a":2})";
    const std::string out = R"({"a":2,"b":1,"vals":[2,1]})";
    EXPECT_EQ(jt::values(doc(in), "/", "/vals").to_json(), out);
}

TEST(JtValues, FormBAtRootIsAnError) {
    EXPECT_THROW(jt::values(doc(R"({"a":1})"), "/", "/"), jt::Error);
}

// --- the composability round-trip ---

TEST(RoundTrip, KeysValuesZipRebuildsTheObject) {
    // Split into keys + values (Form B, document intact), then jtZip would
    // recombine. Here we assert the two halves line up positionally.
    const std::string in = R"({"b":1,"a":2})";
    jsom::JsonDocument keys = jt::keys(jsom::JsonDocument(doc(in)), "/");
    jsom::JsonDocument vals = jt::values_array(jsom::JsonDocument(doc(in)), "/");
    EXPECT_EQ(keys.to_json(), R"(["a","b"])");
    EXPECT_EQ(vals.to_json(), R"([2,1])");
    EXPECT_EQ(keys.size(), vals.size());
}

} // namespace
