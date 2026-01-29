#include "body_handler.hpp"
#include <algorithm>
#include <gtest/gtest.h>

using namespace gwmilter;

// Test smime_body_handler's get_headers() method
// (encrypt() tests require mocking crypto and will come in Phase 2)
class SmimeBodyHandlerHeadersTest : public ::testing::Test {
protected:
    smime_body_handler handler;

    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersReturnsPkcs7MimeType)
{
    headers_type headers = handler.get_headers();

    // Should have at least the Content-Type header
    ASSERT_FALSE(headers.empty());

    // Find Content-Type header
    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // Check it contains application/pkcs7-mime
    EXPECT_NE(it->value.find("application/pkcs7-mime"), std::string::npos);
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersIncludesSmimeType)
{
    headers_type headers = handler.get_headers();

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // Should have smime-type=enveloped-data
    EXPECT_NE(it->value.find("smime-type=enveloped-data"), std::string::npos);
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersIncludesFilename)
{
    headers_type headers = handler.get_headers();

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // Should have name="smime.p7m"
    EXPECT_NE(it->value.find("name=\"smime.p7m\""), std::string::npos);
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersAddsTransferEncoding)
{
    headers_type headers = handler.get_headers();

    // Should have Content-Transfer-Encoding header
    auto it = std::find_if(headers.begin(), headers.end(),
                           [](const header_item &h) { return h.name == "Content-Transfer-Encoding"; });
    ASSERT_NE(it, headers.end());
    EXPECT_EQ(it->value, "base64");
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersAddsContentDisposition)
{
    headers_type headers = handler.get_headers();

    // Should have Content-Disposition header
    auto it = std::find_if(headers.begin(), headers.end(),
                           [](const header_item &h) { return h.name == "Content-Disposition"; });
    ASSERT_NE(it, headers.end());
    EXPECT_NE(it->value.find("attachment"), std::string::npos);
    EXPECT_NE(it->value.find("smime.p7m"), std::string::npos);
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersAddsContentDescription)
{
    headers_type headers = handler.get_headers();

    // Should have Content-Description header
    auto it = std::find_if(headers.begin(), headers.end(),
                           [](const header_item &h) { return h.name == "Content-Description"; });
    ASSERT_NE(it, headers.end());
    EXPECT_NE(it->value.find("S/MIME Encrypted Message"), std::string::npos);
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersUpdatesExistingContentType)
{
    // Add an existing Content-Type header
    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom", "value");

    headers_type headers = handler.get_headers();

    // Content-Type should be updated to application/pkcs7-mime
    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());
    EXPECT_NE(it->value.find("application/pkcs7-mime"), std::string::npos);

    // X-Custom should remain unchanged
    auto custom_it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "X-Custom"; });
    ASSERT_NE(custom_it, headers.end());
    EXPECT_EQ(custom_it->value, "value");
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersOnlyAddsExtraHeadersOnce)
{
    // Call get_headers() multiple times
    headers_type headers1 = handler.get_headers();
    size_t count1 = headers1.size();

    headers_type headers2 = handler.get_headers();
    size_t count2 = headers2.size();

    // Should not duplicate the extra headers
    EXPECT_EQ(count1, count2);

    // Count Content-Transfer-Encoding headers (should be exactly 1)
    int transfer_encoding_count = std::count_if(
        headers2.begin(), headers2.end(), [](const header_item &h) { return h.name == "Content-Transfer-Encoding"; });
    EXPECT_EQ(transfer_encoding_count, 1);

    // Count Content-Disposition headers (should be exactly 1)
    int disposition_count = std::count_if(headers2.begin(), headers2.end(),
                                          [](const header_item &h) { return h.name == "Content-Disposition"; });
    EXPECT_EQ(disposition_count, 1);

    // Count Content-Description headers (should be exactly 1)
    int description_count = std::count_if(headers2.begin(), headers2.end(),
                                          [](const header_item &h) { return h.name == "Content-Description"; });
    EXPECT_EQ(description_count, 1);
}

TEST_F(SmimeBodyHandlerHeadersTest, GetHeadersMarksHeadersAsModified)
{
    headers_type headers = handler.get_headers();

    // All headers created by get_headers() should be marked as modified
    auto content_type =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(content_type, headers.end());
    EXPECT_TRUE(content_type->modified);

    auto transfer_encoding = std::find_if(headers.begin(), headers.end(),
                                          [](const header_item &h) { return h.name == "Content-Transfer-Encoding"; });
    ASSERT_NE(transfer_encoding, headers.end());
    EXPECT_TRUE(transfer_encoding->modified);
}
