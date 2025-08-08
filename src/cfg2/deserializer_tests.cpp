#include "core.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace cfg2;

class DeserializerTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(DeserializerTest, DeserializeGeneralSection)
{
    ConfigNode node{"general",
                    "",
                    {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                     {"daemonize", "true", {}, NodeType::VALUE},
                     {"log_type", "syslog", {}, NodeType::VALUE},
                     {"smtp_server", "smtp://test.example.com", {}, NodeType::VALUE},
                     {"milter_timeout", "300", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    GeneralSection result = deserialize<GeneralSection>(node);

    EXPECT_EQ(result.milter_socket, "unix:/tmp/test.sock");
    EXPECT_EQ(result.daemonize, true);
    EXPECT_EQ(result.log_type, "syslog");
    EXPECT_EQ(result.smtp_server, "smtp://test.example.com");
    EXPECT_EQ(result.milter_timeout, 300);
}

TEST_F(DeserializerTest, DeserializeGeneralSectionWithDefaults)
{
    ConfigNode node{"general",
                    "",
                    {
                        {"milter_socket", "unix:/tmp/minimal.sock", {}, NodeType::VALUE},
                        {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}
                        // Missing other fields - should use defaults
                    },
                    NodeType::SECTION};

    GeneralSection result = deserialize<GeneralSection>(node);

    EXPECT_EQ(result.milter_socket, "unix:/tmp/minimal.sock");
    EXPECT_EQ(result.daemonize, false); // default
    EXPECT_EQ(result.log_type, "console"); // default
    EXPECT_EQ(result.smtp_server, "smtp://localhost");
    EXPECT_EQ(result.milter_timeout, -1); // default
}

TEST_F(DeserializerTest, DeserializePgpEncryptionSection)
{
    ConfigNode node{"pgp_section",
                    "",
                    {{"match", ".*@secure\\.com,admin@.*", {}, NodeType::VALUE},
                     {"encryption_protocol", "pgp", {}, NodeType::VALUE},
                     {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    PgpEncryptionSection result = deserialize<PgpEncryptionSection>(node);

    EXPECT_EQ(result.match.size(), 2);
    EXPECT_EQ(result.match[0], ".*@secure\\.com");
    EXPECT_EQ(result.match[1], "admin@.*");
    EXPECT_EQ(result.encryption_protocol, "pgp");
    EXPECT_EQ(result.key_not_found_policy, "reject");
}

TEST_F(DeserializerTest, DeserializePdfEncryptionSection)
{
    ConfigNode node{"pdf_section",
                    "",
                    {{"match", ".*@external\\.org", {}, NodeType::VALUE},
                     {"encryption_protocol", "pdf", {}, NodeType::VALUE},
                     {"pdf_font_size", "14.5", {}, NodeType::VALUE},
                     {"pdf_margin", "20.0", {}, NodeType::VALUE},
                     {"pdf_attachment", "secure_email.pdf", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    PdfEncryptionSection result = deserialize<PdfEncryptionSection>(node);

    EXPECT_EQ(result.match.size(), 1);
    EXPECT_EQ(result.match[0], ".*@external\\.org");
    EXPECT_EQ(result.encryption_protocol, "pdf");
    EXPECT_FLOAT_EQ(result.pdf_font_size, 14.5f);
    EXPECT_FLOAT_EQ(result.pdf_margin, 20.0f);
    EXPECT_EQ(result.pdf_attachment, "secure_email.pdf");
}

TEST_F(DeserializerTest, ParseFunctionWorksWithConfig)
{
    ConfigNode node{"config",
                    "",
                    {{"general",
                      "",
                      {{"milter_socket", "unix:/parse/test.sock", {}, NodeType::VALUE},
                       {"log_type", "console", {}, NodeType::VALUE},
                       {"smtp_server", "smtp://parse.test.com", {}, NodeType::VALUE}},
                      NodeType::SECTION}},
                    NodeType::ROOT};

    Config result = parse<Config>(node);

    EXPECT_EQ(result.general.milter_socket, "unix:/parse/test.sock");
    EXPECT_EQ(result.general.log_type, "console");
    EXPECT_EQ(result.general.smtp_server, "smtp://parse.test.com");
}