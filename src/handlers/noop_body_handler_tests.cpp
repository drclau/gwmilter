#include "body_handler.hpp"
#include <gtest/gtest.h>

using namespace gwmilter;

class NoopBodyHandlerTest : public ::testing::Test {
protected:
    noop_body_handler handler;

    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(NoopBodyHandlerTest, WriteAccumulatesData)
{
    handler.write("First chunk");
    handler.write(" Second chunk");
    handler.write(" Third chunk");

    std::string output;
    handler.encrypt({}, output);

    EXPECT_EQ(output, "First chunk Second chunk Third chunk");
}

TEST_F(NoopBodyHandlerTest, EncryptSwapsDataToOutput)
{
    handler.write("Test data");

    std::string output;
    handler.encrypt({}, output);

    EXPECT_EQ(output, "Test data");

    // After encrypt, internal data should be empty (swapped)
    std::string second_output;
    handler.encrypt({}, second_output);
    EXPECT_EQ(second_output, "");
}

TEST_F(NoopBodyHandlerTest, GetHeadersReturnsStoredHeaders)
{
    // noop_body_handler inherits from body_handler_base
    // which stores headers added via add_header
    handler.add_header("Content-Type", "text/plain");
    handler.add_header("X-Custom-Header", "test-value");

    headers_type headers = handler.get_headers();

    EXPECT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].name, "Content-Type");
    EXPECT_EQ(headers[0].value, "text/plain");
    EXPECT_EQ(headers[1].name, "X-Custom-Header");
    EXPECT_EQ(headers[1].value, "test-value");
}

TEST_F(NoopBodyHandlerTest, HasPublicKeyAlwaysReturnsTrue)
{
    // noop_body_handler always returns true (pass-through, no PKI)
    EXPECT_TRUE(handler.has_public_key("anyone@example.com"));
    EXPECT_TRUE(handler.has_public_key(""));
    EXPECT_TRUE(handler.has_public_key("invalid-email"));
}

TEST_F(NoopBodyHandlerTest, ImportPublicKeyAlwaysReturnsTrue)
{
    // noop_body_handler always returns true (pass-through, no PKI)
    EXPECT_TRUE(handler.import_public_key("anyone@example.com"));
    EXPECT_TRUE(handler.import_public_key(""));
    EXPECT_TRUE(handler.import_public_key("invalid-email"));
}

TEST_F(NoopBodyHandlerTest, EncryptIgnoresRecipients)
{
    handler.write("Message content");

    std::set<std::string> recipients{"alice@example.com", "bob@example.com"};
    std::string output;
    handler.encrypt(recipients, output);

    // Recipients are ignored; output is just the accumulated data
    EXPECT_EQ(output, "Message content");
}
