#include "config.hpp"
#include "core.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace cfg2;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

class ConfigValidationTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

// Test Config structure and find_match functionality
TEST_F(ConfigTest, ConfigFindMatchWorksCorrectly)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/var/run/gwmilter/gwmilter.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"encrypt_internal",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@company\\.com,.*@internal\\.org", {}, NodeType::VALUE},
           {"key_not_found_policy", "retrieve", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"encrypt_external",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE},
           {"match", ".*@external\\.com", {}, NodeType::VALUE},
           {"pdf_font_size", "12.5", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"no_encryption",
          "",
          {{"encryption_protocol", "none", {}, NodeType::VALUE}, {"match", ".*@public\\.org", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    Config config = parse<Config>(configNode);

    // Test successful matches
    auto *internal = config.find_match("user@company.com");
    EXPECT_NE(internal, nullptr);
    EXPECT_EQ(internal->encryption_protocol, "pgp");

    auto *external = config.find_match("client@external.com");
    EXPECT_NE(external, nullptr);
    EXPECT_EQ(external->encryption_protocol, "pdf");

    auto *none = config.find_match("newsletter@public.org");
    EXPECT_NE(none, nullptr);
    EXPECT_EQ(none->encryption_protocol, "none");

    // Test no match
    auto *noMatch = config.find_match("user@nowhere.com");
    EXPECT_EQ(noMatch, nullptr);
}

TEST_F(ConfigTest, ConfigDeserializesGeneralSection)
{
    ConfigNode configNode{"config",
                          "",
                          {{"general",
                            "",
                            {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                             {"daemonize", "true", {}, NodeType::VALUE},
                             {"log_type", "syslog", {}, NodeType::VALUE},
                             {"log_facility", "daemon", {}, NodeType::VALUE},
                             {"milter_timeout", "300", {}, NodeType::VALUE},
                             {"smtp_server", "smtp://mail.example.com:587", {}, NodeType::VALUE}},
                            NodeType::SECTION}},
                          NodeType::ROOT};

    Config config = parse<Config>(configNode);

    EXPECT_EQ(config.general.milter_socket, "unix:/tmp/test.sock");
    EXPECT_EQ(config.general.daemonize, true);
    EXPECT_EQ(config.general.log_type, "syslog");
    EXPECT_EQ(config.general.log_facility, "daemon");
    EXPECT_EQ(config.general.milter_timeout, 300);
    EXPECT_EQ(config.general.smtp_server, "smtp://mail.example.com:587");
}

TEST_F(ConfigTest, ConfigHandlesMultipleEncryptionSections)
{
    ConfigNode configNode{"config",
                          "",
                          {{"general",
                            "",
                            {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                             {"log_type", "console", {}, NodeType::VALUE},
                             {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                            NodeType::SECTION},
                           {"pgp1",
                            "",
                            {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
                             {"match", ".*@secure\\.com", {}, NodeType::VALUE},
                             {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                            NodeType::SECTION},
                           {"pdf1",
                            "",
                            {{"encryption_protocol", "pdf", {}, NodeType::VALUE},
                             {"match", ".*@external\\.org", {}, NodeType::VALUE},
                             {"pdf_font_size", "14.0", {}, NodeType::VALUE}},
                            NodeType::SECTION},
                           {"pgp2",
                            "",
                            {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
                             {"match", ".*@backup\\.net", {}, NodeType::VALUE},
                             {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                            NodeType::SECTION}},
                          NodeType::ROOT};

    Config config = parse<Config>(configNode);

    EXPECT_EQ(config.encryptionSections.size(), 3);

    // Test order is preserved
    EXPECT_EQ(config.encryptionSections[0]->encryption_protocol, "pgp");
    EXPECT_EQ(config.encryptionSections[1]->encryption_protocol, "pdf");
    EXPECT_EQ(config.encryptionSections[2]->encryption_protocol, "pgp");

    // Test find_match returns first match
    auto *match = config.find_match("test@secure.com");
    EXPECT_NE(match, nullptr);
    EXPECT_EQ(match->sectionName, "pgp1");
}

// Validation tests
TEST_F(ConfigValidationTest, ValidConfigurationParsesSuccessfully)
{
    ConfigNode validConfig{"config",
                           "",
                           {{"general",
                             "",
                             {{"milter_socket", "unix:/var/run/gwmilter/gwmilter.sock", {}, NodeType::VALUE},
                              {"log_type", "console", {}, NodeType::VALUE},
                              {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE}},
                             NodeType::SECTION}},
                           NodeType::ROOT};

    EXPECT_NO_THROW({ Config config = parse<Config>(validConfig); });
}

TEST_F(ConfigValidationTest, InvalidLogTypeThrowsException)
{
    ConfigNode invalidLogType{"config",
                              "",
                              {{"general",
                                "",
                                {{"milter_socket", "unix:/var/run/gwmilter/gwmilter.sock", {}, NodeType::VALUE},
                                 {"log_type", "invalid_type", {}, NodeType::VALUE},
                                 {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE}},
                                NodeType::SECTION}},
                              NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidLogType); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, EmptyMilterSocketThrowsException)
{
    ConfigNode emptySocket{"config",
                           "",
                           {{"general",
                             "",
                             {{"milter_socket", "", {}, NodeType::VALUE},
                              {"log_type", "console", {}, NodeType::VALUE},
                              {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE}},
                             NodeType::SECTION}},
                           NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(emptySocket); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, InvalidSmtpServerThrowsException)
{
    ConfigNode invalidSmtp{"config",
                           "",
                           {{"general",
                             "",
                             {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                              {"log_type", "console", {}, NodeType::VALUE},
                              {"smtp_server", "invalid_url", {}, NodeType::VALUE}},
                             NodeType::SECTION}},
                           NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidSmtp); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, InvalidTimeoutThrowsException)
{
    ConfigNode invalidTimeout{"config",
                              "",
                              {{"general",
                                "",
                                {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                                 {"log_type", "console", {}, NodeType::VALUE},
                                 {"smtp_server", "smtp://localhost", {}, NodeType::VALUE},
                                 {"milter_timeout", "-5", {}, NodeType::VALUE}},
                                NodeType::SECTION}},
                              NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidTimeout); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, InvalidPdfFontSizeThrowsException)
{
    ConfigNode invalidPdfSize{"config",
                              "",
                              {{"general",
                                "",
                                {{"milter_socket", "unix:/var/run/gwmilter/gwmilter.sock", {}, NodeType::VALUE},
                                 {"log_type", "console", {}, NodeType::VALUE},
                                 {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE}},
                                NodeType::SECTION},
                               {"encrypt_pdf",
                                "",
                                {{"encryption_protocol", "pdf", {}, NodeType::VALUE},
                                 {"match", ".*@test\\.com", {}, NodeType::VALUE},
                                 {"pdf_font_size", "0", {}, NodeType::VALUE}},
                                NodeType::SECTION}},
                              NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidPdfSize); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, MissingMatchPatternsThrowsException)
{
    ConfigNode noMatchPatterns{"config",
                               "",
                               {{"general",
                                 "",
                                 {{"milter_socket", "unix:/var/run/gwmilter/gwmilter.sock", {}, NodeType::VALUE},
                                  {"log_type", "console", {}, NodeType::VALUE},
                                  {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE}},
                                 NodeType::SECTION},
                                {"encrypt_pgp",
                                 "",
                                 {
                                     {"encryption_protocol", "pgp", {}, NodeType::VALUE}
                                     // Missing match patterns
                                 },
                                 NodeType::SECTION}},
                               NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(noMatchPatterns); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, InvalidKeyNotFoundPolicyThrowsException)
{
    ConfigNode invalidPolicy{"config",
                             "",
                             {{"general",
                               "",
                               {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                                {"log_type", "console", {}, NodeType::VALUE},
                                {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                               NodeType::SECTION},
                              {"encrypt_pgp",
                               "",
                               {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
                                {"match", ".*@test\\.com", {}, NodeType::VALUE},
                                {"key_not_found_policy", "invalid_policy", {}, NodeType::VALUE}},
                               NodeType::SECTION}},
                             NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidPolicy); }, std::invalid_argument);
}