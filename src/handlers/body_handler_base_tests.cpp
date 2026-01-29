#include "body_handler.hpp"
#include <cctype>
#include <gtest/gtest.h>

using namespace gwmilter;

// Tests for body_handler_base protected methods.
// Access is granted via FRIEND_TEST declarations in body_handler.hpp.
class BodyHandlerBaseTest : public ::testing::Test {
protected:
    noop_body_handler handler;
};

// ============================================
// generate_boundary tests
// ============================================

TEST_F(BodyHandlerBaseTest, GenerateBoundaryReturnsCorrectLength)
{
    std::string boundary = body_handler_base::generate_boundary();
    EXPECT_EQ(boundary.length(), 70);

    std::string short_boundary = body_handler_base::generate_boundary(10);
    EXPECT_EQ(short_boundary.length(), 10);

    std::string long_boundary = body_handler_base::generate_boundary(100);
    EXPECT_EQ(long_boundary.length(), 100);
}

TEST_F(BodyHandlerBaseTest, GenerateBoundaryContainsOnlyValidChars)
{
    std::string boundary = body_handler_base::generate_boundary(100);

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
    handler.add_header("Received", "from server1");
    handler.add_header("Received", "from server2");
    handler.add_header("X-Other", "value");
    handler.add_header("Received", "from server3");

    headers_type headers = handler.get_headers();

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
    noop_body_handler handler;
};

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersFindsContentType)
{
    handler.add_header("Content-Type", "text/html; charset=utf-8");
    handler.add_header("X-Custom", "value");
    handler.add_header("Content-Transfer-Encoding", "quoted-printable");

    body_handler_base &base = handler;
    headers_type content_headers;
    std::string content_type = base.extract_content_headers(content_headers);

    EXPECT_EQ(content_type, "text/html; charset=utf-8");

    EXPECT_EQ(content_headers.size(), 2);
    EXPECT_EQ(content_headers[0].name, "Content-Type");
    EXPECT_EQ(content_headers[1].name, "Content-Transfer-Encoding");
}

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersCaseInsensitive)
{
    handler.add_header("content-type", "text/plain");
    handler.add_header("CONTENT-ENCODING", "gzip");
    handler.add_header("Content-Disposition", "attachment");

    body_handler_base &base = handler;
    headers_type content_headers;
    std::string content_type = base.extract_content_headers(content_headers);

    EXPECT_EQ(content_type, "text/plain");
    EXPECT_EQ(content_headers.size(), 3);
}

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersMarksAsModified)
{
    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom", "value");

    body_handler_base &base = handler;
    headers_type content_headers;
    base.extract_content_headers(content_headers);

    headers_type all_headers = handler.get_headers();

    EXPECT_TRUE(all_headers[0].modified);
    EXPECT_TRUE(all_headers[0].value.empty());

    EXPECT_FALSE(all_headers[1].modified);
    EXPECT_EQ(all_headers[1].value, "value");
}

TEST_F(ExtractContentHeadersTest, ExtractContentHeadersHandlesNoContentHeaders)
{
    handler.add_header("X-Custom", "value");
    handler.add_header("Received", "from somewhere");

    body_handler_base &base = handler;
    headers_type content_headers;
    std::string content_type = base.extract_content_headers(content_headers);

    EXPECT_TRUE(content_type.empty());
    EXPECT_TRUE(content_headers.empty());
}
