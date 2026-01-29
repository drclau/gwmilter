#include "body_handler.hpp"
#include <cctype>
#include <gtest/gtest.h>

using namespace gwmilter;

// Helper class to expose protected methods for testing
class TestableBodyHandler : public body_handler_base {
public:
    headers_type get_headers() override { return headers_; }
    void encrypt(const recipients_type &, std::string &) override { }
    bool has_public_key(const std::string &) const override { return true; }
    bool import_public_key(const std::string &) override { return true; }

    // Expose protected methods
    static std::string public_generate_boundary(std::size_t length = 70) { return generate_boundary(length); }

    std::string public_extract_content_headers(headers_type &content_headers)
    {
        return extract_content_headers(content_headers);
    }
};

// Test body_handler_base protected methods via helper subclass
class BodyHandlerBaseTest : public ::testing::Test {
protected:
    TestableBodyHandler handler;

    void SetUp() override { }
    void TearDown() override { }
};

// ============================================
// generate_boundary tests
// ============================================

TEST_F(BodyHandlerBaseTest, GenerateBoundaryReturnsCorrectLength)
{
    // Test default length (70)
    std::string boundary = TestableBodyHandler::public_generate_boundary();
    EXPECT_EQ(boundary.length(), 70);

    // Test custom lengths
    std::string short_boundary = TestableBodyHandler::public_generate_boundary(10);
    EXPECT_EQ(short_boundary.length(), 10);

    std::string long_boundary = TestableBodyHandler::public_generate_boundary(100);
    EXPECT_EQ(long_boundary.length(), 100);
}

TEST_F(BodyHandlerBaseTest, GenerateBoundaryContainsOnlyValidChars)
{
    std::string boundary = TestableBodyHandler::public_generate_boundary(100);

    // RFC 2046 allows alphanumeric characters for boundaries
    for (char c: boundary)
        EXPECT_TRUE(std::isalnum(static_cast<unsigned char>(c))) << "Boundary contains invalid character: " << c;
}

// ============================================
// add_header tests
// ============================================

TEST_F(BodyHandlerBaseTest, AddHeaderStoresHeaderCorrectly)
{
    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom", "value");

    headers_type headers = handler.get_headers();

    EXPECT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].name, "Content-Type");
    EXPECT_EQ(headers[0].value, "text/plain");
    EXPECT_EQ(headers[1].name, "X-Custom");
    EXPECT_EQ(headers[1].value, "value");
}

TEST_F(BodyHandlerBaseTest, AddHeaderTracksIndexPerName)
{
    // Add multiple headers with the same name
    handler.add_header("Received", "from server1");
    handler.add_header("Received", "from server2");
    handler.add_header("X-Other", "value");
    handler.add_header("Received", "from server3");

    headers_type headers = handler.get_headers();

    // Check that indices increment per header name
    EXPECT_EQ(headers[0].name, "Received");
    EXPECT_EQ(headers[0].index, 1);

    EXPECT_EQ(headers[1].name, "Received");
    EXPECT_EQ(headers[1].index, 2);

    EXPECT_EQ(headers[2].name, "X-Other");
    EXPECT_EQ(headers[2].index, 1);

    EXPECT_EQ(headers[3].name, "Received");
    EXPECT_EQ(headers[3].index, 3);
}

// ============================================
// extract_content_headers tests
// ============================================

class ExtractContentHeadersTest : public ::testing::Test {
protected:
    TestableBodyHandler handler;

    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersFindsContentType)
{
    handler.add_header("Content-Type", "text/html; charset=utf-8");
    handler.add_header("X-Custom", "value");
    handler.add_header("Content-Transfer-Encoding", "quoted-printable");

    headers_type content_headers;
    std::string content_type = handler.public_extract_content_headers(content_headers);

    // Should return Content-Type value in lowercase
    EXPECT_EQ(content_type, "text/html; charset=utf-8");

    // Should extract both Content-* headers
    EXPECT_EQ(content_headers.size(), 2);
    EXPECT_EQ(content_headers[0].name, "Content-Type");
    EXPECT_EQ(content_headers[1].name, "Content-Transfer-Encoding");
}

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersCaseInsensitive)
{
    handler.add_header("content-type", "text/plain");
    handler.add_header("CONTENT-ENCODING", "gzip");
    handler.add_header("Content-Disposition", "attachment");

    headers_type content_headers;
    std::string content_type = handler.public_extract_content_headers(content_headers);

    EXPECT_EQ(content_type, "text/plain");
    EXPECT_EQ(content_headers.size(), 3);
}

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersMarksAsModified)
{
    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom", "value");

    headers_type content_headers;
    handler.public_extract_content_headers(content_headers);

    // Get all headers and check modification status
    headers_type all_headers = handler.get_headers();

    // Content-Type should be marked as modified
    EXPECT_TRUE(all_headers[0].modified);
    EXPECT_TRUE(all_headers[0].value.empty());

    // X-Custom should not be modified
    EXPECT_FALSE(all_headers[1].modified);
    EXPECT_EQ(all_headers[1].value, "value");
}

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersHandlesNoContentHeaders)
{
    handler.add_header("X-Custom", "value");
    handler.add_header("Received", "from somewhere");

    headers_type content_headers;
    std::string content_type = handler.public_extract_content_headers(content_headers);

    EXPECT_TRUE(content_type.empty());
    EXPECT_TRUE(content_headers.empty());
}
