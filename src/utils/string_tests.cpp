#include "string.hpp"
#include <gtest/gtest.h>

using namespace gwmilter::utils::string;

// ============================================
// to_lower tests
// ============================================

TEST(StringUtilsTest, ToLowerConvertsUppercase)
{
    EXPECT_EQ(to_lower("HELLO"), "hello");
    EXPECT_EQ(to_lower("WORLD"), "world");
    EXPECT_EQ(to_lower("ABC123XYZ"), "abc123xyz");
}

TEST(StringUtilsTest, ToLowerPreservesNonAlpha)
{
    EXPECT_EQ(to_lower("Test-123!@#"), "test-123!@#");
    EXPECT_EQ(to_lower("user@DOMAIN.COM"), "user@domain.com");
}

// ============================================
// iequals tests
// ============================================

TEST(StringUtilsTest, IequalsMatchesDifferentCase)
{
    EXPECT_TRUE(iequals("Hello", "hello"));
    EXPECT_TRUE(iequals("WORLD", "world"));
    EXPECT_TRUE(iequals("Content-Type", "content-type"));
}

TEST(StringUtilsTest, IequalsReturnsFalseForDifferentStrings)
{
    EXPECT_FALSE(iequals("hello", "world"));
    EXPECT_FALSE(iequals("test", "testing"));
}

TEST(StringUtilsTest, IequalsHandlesEmptyStrings)
{
    EXPECT_TRUE(iequals("", ""));
    EXPECT_FALSE(iequals("", "nonempty"));
    EXPECT_FALSE(iequals("nonempty", ""));
}

// ============================================
// set_to_string tests
// ============================================

TEST(StringUtilsTest, SetToStringSingleElement)
{
    std::set<std::string> single_set{"alice@example.com"};
    EXPECT_EQ(set_to_string(single_set), "alice@example.com");
}

TEST(StringUtilsTest, SetToStringMultipleElements)
{
    std::set<std::string> multi_set{"alice@example.com", "bob@example.com", "charlie@example.com"};
    // Sets are ordered, so we can predict the output
    EXPECT_EQ(set_to_string(multi_set), "alice@example.com, bob@example.com, charlie@example.com");
}
