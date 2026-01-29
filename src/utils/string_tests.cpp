#include "string.hpp"
#include <gtest/gtest.h>

using namespace gwmilter::utils::string;

class StringUtilsTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

// ============================================
// to_lower tests
// ============================================

TEST_F(StringUtilsTest, ToLowerConvertsUppercase)
{
    EXPECT_EQ(to_lower("HELLO"), "hello");
    EXPECT_EQ(to_lower("WORLD"), "world");
    EXPECT_EQ(to_lower("ABC123XYZ"), "abc123xyz");
}

TEST_F(StringUtilsTest, ToLowerHandlesEmptyString)
{
    EXPECT_EQ(to_lower(""), "");
}

TEST_F(StringUtilsTest, ToLowerHandlesMixedCase)
{
    EXPECT_EQ(to_lower("HeLLo WoRLd"), "hello world");
    EXPECT_EQ(to_lower("CamelCase"), "camelcase");
}

TEST_F(StringUtilsTest, ToLowerPreservesNonAlpha)
{
    EXPECT_EQ(to_lower("Test-123!@#"), "test-123!@#");
    EXPECT_EQ(to_lower("user@DOMAIN.COM"), "user@domain.com");
}

// ============================================
// iequals tests
// ============================================

TEST_F(StringUtilsTest, IequalsMatchesSameCase)
{
    EXPECT_TRUE(iequals("hello", "hello"));
    EXPECT_TRUE(iequals("WORLD", "WORLD"));
}

TEST_F(StringUtilsTest, IequalsMatchesDifferentCase)
{
    EXPECT_TRUE(iequals("Hello", "hello"));
    EXPECT_TRUE(iequals("WORLD", "world"));
    EXPECT_TRUE(iequals("Content-Type", "content-type"));
}

TEST_F(StringUtilsTest, IequalsReturnsFalseForDifferentStrings)
{
    EXPECT_FALSE(iequals("hello", "world"));
    EXPECT_FALSE(iequals("test", "testing"));
}

TEST_F(StringUtilsTest, IequalsHandlesEmptyStrings)
{
    EXPECT_TRUE(iequals("", ""));
    EXPECT_FALSE(iequals("", "nonempty"));
    EXPECT_FALSE(iequals("nonempty", ""));
}

TEST_F(StringUtilsTest, IequalsReturnsFalseForDifferentLengths)
{
    EXPECT_FALSE(iequals("short", "longer"));
    EXPECT_FALSE(iequals("abc", "ab"));
}

// ============================================
// set_to_string tests
// ============================================

TEST_F(StringUtilsTest, SetToStringFormatsEmptySet)
{
    std::set<std::string> empty_set;
    EXPECT_EQ(set_to_string(empty_set), "");
}

TEST_F(StringUtilsTest, SetToStringSingleElement)
{
    std::set<std::string> single_set{"alice@example.com"};
    EXPECT_EQ(set_to_string(single_set), "alice@example.com");
}

TEST_F(StringUtilsTest, SetToStringMultipleElements)
{
    std::set<std::string> multi_set{"alice@example.com", "bob@example.com", "charlie@example.com"};
    // Sets are ordered, so we can predict the output
    EXPECT_EQ(set_to_string(multi_set), "alice@example.com, bob@example.com, charlie@example.com");
}

// ============================================
// str_err tests
// ============================================

TEST_F(StringUtilsTest, StrErrReturnsValidMessage)
{
    // ENOENT should return a non-empty message
    std::string msg = str_err(ENOENT);
    EXPECT_FALSE(msg.empty());
    // The message should contain some indication of "no such file"
    std::string lower_msg = to_lower(msg);
    EXPECT_TRUE(lower_msg.find("no such") != std::string::npos || lower_msg.find("not found") != std::string::npos ||
                lower_msg.find("file") != std::string::npos);
}
