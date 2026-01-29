#include "body_handler.hpp"
#include <algorithm>
#include <gtest/gtest.h>

using namespace gwmilter;

// Test pgp_body_handler's get_headers() method
// (encrypt() tests require mocking crypto and will come in Phase 2)
class PgpBodyHandlerHeadersTest : public ::testing::Test {
protected:
    pgp_body_handler handler;

    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(PgpBodyHandlerHeadersTest, GetHeadersReturnsMultipartEncrypted)
{
    headers_type headers = handler.get_headers();

    // Should have at least the Content-Type header
    ASSERT_FALSE(headers.empty());

    // Find Content-Type header
    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // Check it contains multipart/encrypted
    EXPECT_NE(it->value.find("multipart/encrypted"), std::string::npos);
}

TEST_F(PgpBodyHandlerHeadersTest, GetHeadersIncludesPgpProtocol)
{
    headers_type headers = handler.get_headers();

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // RFC 3156 requires protocol="application/pgp-encrypted"
    EXPECT_NE(it->value.find("application/pgp-encrypted"), std::string::npos);
    EXPECT_NE(it->value.find("protocol="), std::string::npos);
}

TEST_F(PgpBodyHandlerHeadersTest, GetHeadersIncludesBoundary)
{
    headers_type headers = handler.get_headers();

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // Should have a boundary parameter
    EXPECT_NE(it->value.find("boundary="), std::string::npos);
}

TEST_F(PgpBodyHandlerHeadersTest, GetHeadersUpdatesExistingContentType)
{
    // Add an existing Content-Type header
    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom", "value");

    headers_type headers = handler.get_headers();

    // Should still have 2 headers (Content-Type updated, not duplicated)
    EXPECT_EQ(headers.size(), 2);

    // Content-Type should be updated to multipart/encrypted
    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());
    EXPECT_NE(it->value.find("multipart/encrypted"), std::string::npos);

    // X-Custom should remain unchanged
    auto custom_it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "X-Custom"; });
    ASSERT_NE(custom_it, headers.end());
    EXPECT_EQ(custom_it->value, "value");
}

TEST_F(PgpBodyHandlerHeadersTest, GetHeadersCreatesContentTypeIfMissing)
{
    // Don't add any headers
    headers_type headers = handler.get_headers();

    // Should create Content-Type header
    EXPECT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].name, "Content-Type");
    EXPECT_NE(headers[0].value.find("multipart/encrypted"), std::string::npos);
}

TEST_F(PgpBodyHandlerHeadersTest, GetHeadersMarksContentTypeAsModified)
{
    headers_type headers = handler.get_headers();

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // modified flag should be set to true (index=1, modified=true per code)
    EXPECT_TRUE(it->modified);
}

TEST_F(PgpBodyHandlerHeadersTest, GetHeadersMultipleCallsReturnSameHeaders)
{
    headers_type headers1 = handler.get_headers();
    headers_type headers2 = handler.get_headers();

    // Should return the same headers on multiple calls
    EXPECT_EQ(headers1.size(), headers2.size());

    // Content-Type should be the same
    EXPECT_EQ(headers1[0].name, headers2[0].name);
    EXPECT_EQ(headers1[0].value, headers2[0].value);
}
