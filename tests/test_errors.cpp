// jt::errors — the typo-hint helpers behind `Error at <path>: ...` messages.
#include <gtest/gtest.h>

#include "jt/errors.hpp"

namespace {

TEST(JtErrors, NearestKeySuggestsACloseKey) {
  EXPECT_EQ(jt::nearest_key({"user"}, "usr"), "did you mean /user?");
}

TEST(JtErrors, NearestKeyReturnsNothingWhenNoKeyIsClose) {
  EXPECT_EQ(jt::nearest_key({"alpha", "beta"}, "usr"), "");
}

// review issue 7: a suggested key containing '/' or '~' must be escaped the
// way suggest_for does, or the hint points at a different path entirely
// ("/a/b" is the key "b" inside "a", not the key "a/b").
TEST(JtErrors, NearestKeyEscapesASlashInTheSuggestedKey) {
  EXPECT_EQ(jt::nearest_key({"a/b"}, "axb"), "did you mean /a~1b?");
}

TEST(JtErrors, NearestKeyEscapesATildeInTheSuggestedKey) {
  EXPECT_EQ(jt::nearest_key({"a~b"}, "axb"), "did you mean /a~0b?");
}

}  // namespace
