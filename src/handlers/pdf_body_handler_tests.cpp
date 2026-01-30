#include "body_handler.hpp"
#include "cfg2/config.hpp"
#include <algorithm>
#include <gtest/gtest.h>

using namespace gwmilter;

class PdfBodyHandlerTest : public ::testing::Test {
protected:
    cfg2::PdfEncryptionSection default_settings;

    void SetUp() override
    {
        default_settings.encryption_protocol = cfg2::EncryptionProtocol::Pdf;
        default_settings.match = {".*@example\\.com"};
        default_settings.pdf_attachment = "test.pdf";
        default_settings.pdf_font_size = 12.0f;
        default_settings.pdf_margin = 10.0f;
    }
};

TEST_F(PdfBodyHandlerTest, GetHeadersReturnsMultipartMixed)
{
    pdf_body_handler handler(default_settings);
    headers_type headers = handler.get_headers();

    ASSERT_FALSE(headers.empty());

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    EXPECT_NE(it->value.find("multipart/mixed"), std::string::npos);
}

TEST_F(PdfBodyHandlerTest, GetHeadersUpdatesExistingContentType)
{
    pdf_body_handler handler(default_settings);

    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom", "value");

    headers_type headers = handler.get_headers();

    // Should still have 2 headers (Content-Type updated, not duplicated)
    EXPECT_EQ(headers.size(), 2);

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());
    EXPECT_NE(it->value.find("multipart/mixed"), std::string::npos);
}
