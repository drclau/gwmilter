#include "body_handler.hpp"
#include "cfg2/config.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace gwmilter;

class PdfBodyHandlerTest : public ::testing::Test {
protected:
    std::filesystem::path test_dir;
    cfg2::PdfEncryptionSection default_settings;

    void SetUp() override
    {
        // Create temporary directory for test files
        test_dir = std::filesystem::temp_directory_path() / "gwmilter_pdf_tests";
        std::filesystem::create_directories(test_dir);

        // Set up default settings
        default_settings.encryption_protocol = cfg2::EncryptionProtocol::Pdf;
        default_settings.match = {".*@example\\.com"};
        default_settings.pdf_attachment = "test.pdf";
        default_settings.pdf_font_size = 12.0f;
        default_settings.pdf_margin = 10.0f;
    }

    void TearDown() override
    {
        // Clean up test directory
        if (std::filesystem::exists(test_dir))
            std::filesystem::remove_all(test_dir);
    }

    void create_test_file(const std::string &filename, const std::string &content)
    {
        std::ofstream file(test_dir / filename);
        file << content;
        file.close();
    }
};

// ============================================
// get_headers tests
// ============================================

TEST_F(PdfBodyHandlerTest, GetHeadersReturnsMultipartMixed)
{
    pdf_body_handler handler(default_settings);
    headers_type headers = handler.get_headers();

    // Should have at least the Content-Type header
    ASSERT_FALSE(headers.empty());

    // Find Content-Type header
    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // Check it contains multipart/mixed
    EXPECT_NE(it->value.find("multipart/mixed"), std::string::npos);
}

TEST_F(PdfBodyHandlerTest, GetHeadersIncludesBoundary)
{
    pdf_body_handler handler(default_settings);
    headers_type headers = handler.get_headers();

    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());

    // Should have a boundary parameter
    EXPECT_NE(it->value.find("boundary="), std::string::npos);
}

TEST_F(PdfBodyHandlerTest, GetHeadersUpdatesExistingContentType)
{
    pdf_body_handler handler(default_settings);

    // Add an existing Content-Type header
    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom", "value");

    headers_type headers = handler.get_headers();

    // Should still have 2 headers (Content-Type updated, not duplicated)
    EXPECT_EQ(headers.size(), 2);

    // Content-Type should be updated to multipart/mixed
    auto it =
        std::find_if(headers.begin(), headers.end(), [](const header_item &h) { return h.name == "Content-Type"; });
    ASSERT_NE(it, headers.end());
    EXPECT_NE(it->value.find("multipart/mixed"), std::string::npos);
}

TEST_F(PdfBodyHandlerTest, GetHeadersCreatesContentTypeIfMissing)
{
    pdf_body_handler handler(default_settings);

    // Don't add any headers
    headers_type headers = handler.get_headers();

    // Should create Content-Type header
    EXPECT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].name, "Content-Type");
    EXPECT_NE(headers[0].value.find("multipart/mixed"), std::string::npos);
}

// Note: read_file() is a private static method used internally by pdf_body_handler.
// Since the class is marked final and the method is private, we cannot test it directly.
// Its behavior is tested indirectly through encrypt() tests, which are beyond Phase 1 scope
// and will be added in Phase 2 with mocking support.

// ============================================
// has_public_key / import_public_key tests
// ============================================

TEST_F(PdfBodyHandlerTest, HasPublicKeyAlwaysReturnsTrue)
{
    pdf_body_handler handler(default_settings);

    // PDF encryption doesn't use PKI; always returns true
    EXPECT_TRUE(handler.has_public_key("anyone@example.com"));
    EXPECT_TRUE(handler.has_public_key(""));
    EXPECT_TRUE(handler.has_public_key("invalid-email"));
}

TEST_F(PdfBodyHandlerTest, ImportPublicKeyAlwaysReturnsTrue)
{
    pdf_body_handler handler(default_settings);

    // PDF encryption doesn't use PKI; always returns true
    EXPECT_TRUE(handler.import_public_key("anyone@example.com"));
    EXPECT_TRUE(handler.import_public_key(""));
    EXPECT_TRUE(handler.import_public_key("invalid-email"));
}

// ============================================
// Settings storage tests
// ============================================

TEST_F(PdfBodyHandlerTest, HandlerStoresSettings)
{
    cfg2::PdfEncryptionSection settings;
    settings.encryption_protocol = cfg2::EncryptionProtocol::Pdf;
    settings.match = {".*@test\\.com"};
    settings.pdf_attachment = "custom.pdf";
    settings.pdf_font_path = "/custom/font/path";
    settings.pdf_font_size = 14.5f;
    settings.pdf_margin = 20.0f;
    settings.pdf_password = "secret";
    settings.pdf_main_page_if_missing = "/path/to/main.txt";
    settings.email_body_replacement = "/path/to/replacement.txt";

    // Just verify handler can be constructed with custom settings
    // (actual usage is tested in encrypt(), which is complex and beyond Phase 1 scope)
    EXPECT_NO_THROW({ pdf_body_handler handler(settings); });
}
