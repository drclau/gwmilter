#include "config.hpp"
#include "core.hpp"
#include <algorithm>
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

class MandatorySectionTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

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

TEST_F(ConfigTest, ConfigFindMatchWorksCorrectly)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/var/run/gwmilter/gwmilter.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE},
           {"signing_key", "/path/to/key", {}, NodeType::VALUE}},
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
    EXPECT_EQ(internal->encryption_protocol, EncryptionProtocol::Pgp);

    auto *external = config.find_match("client@external.com");
    EXPECT_NE(external, nullptr);
    EXPECT_EQ(external->encryption_protocol, EncryptionProtocol::Pdf);

    auto *none = config.find_match("newsletter@public.org");
    EXPECT_NE(none, nullptr);
    EXPECT_EQ(none->encryption_protocol, EncryptionProtocol::None);

    // Test no match
    auto *noMatch = config.find_match("user@nowhere.com");
    EXPECT_EQ(noMatch, nullptr);
}

TEST_F(ConfigTest, FindMatchReturnsSecondSection)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://localhost", {}, NodeType::VALUE},
           {"signing_key", "/path/to/key", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"first_section",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@first\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"second_section",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE}, {"match", ".*@second\\.com", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"third_section",
          "",
          {{"encryption_protocol", "none", {}, NodeType::VALUE}, {"match", ".*@third\\.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    Config config = parse<Config>(configNode);

    // Test that second section is matched correctly
    auto *match = config.find_match("user@second.com");
    EXPECT_NE(match, nullptr);
    EXPECT_EQ(match->encryption_protocol, EncryptionProtocol::Pdf);
    EXPECT_EQ(match->sectionName, "second_section");

    // Test that third section is matched correctly
    auto *thirdMatch = config.find_match("user@third.com");
    EXPECT_NE(thirdMatch, nullptr);
    EXPECT_EQ(thirdMatch->encryption_protocol, EncryptionProtocol::None);
    EXPECT_EQ(thirdMatch->sectionName, "third_section");
}

TEST_F(ConfigTest, FindMatchWithEmptyEncryptionSections)
{
    ConfigNode configNode{"config",
                          "",
                          {{"general",
                            "",
                            {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                             {"log_type", "console", {}, NodeType::VALUE},
                             {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                            NodeType::SECTION}},
                          NodeType::ROOT};

    Config config = parse<Config>(configNode);

    EXPECT_EQ(config.encryptionSections.size(), 0);
    auto *match = config.find_match("user@anywhere.com");
    EXPECT_EQ(match, nullptr);
}

TEST_F(ConfigTest, FindMatchReturnsFirstMatchOnly)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://localhost", {}, NodeType::VALUE},
           {"signing_key", "/path/to/key", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"first_match",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@test\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"second_match",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE}, {"match", ".*@test\\.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    Config config = parse<Config>(configNode);

    // Both sections match, but should return first one
    auto *match = config.find_match("user@test.com");
    EXPECT_NE(match, nullptr);
    EXPECT_EQ(match->encryption_protocol, EncryptionProtocol::Pgp);
    EXPECT_EQ(match->sectionName, "first_match");
}

TEST_F(ConfigTest, ConfigHandlesMultipleEncryptionSections)
{
    ConfigNode configNode{"config",
                          "",
                          {{"general",
                            "",
                            {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                             {"log_type", "console", {}, NodeType::VALUE},
                             {"smtp_server", "smtp://localhost", {}, NodeType::VALUE},
                             {"signing_key", "/path/to/key", {}, NodeType::VALUE}},
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
    EXPECT_EQ(config.encryptionSections[0]->encryption_protocol, EncryptionProtocol::Pgp);
    EXPECT_EQ(config.encryptionSections[1]->encryption_protocol, EncryptionProtocol::Pdf);
    EXPECT_EQ(config.encryptionSections[2]->encryption_protocol, EncryptionProtocol::Pgp);

    // Test find_match returns first match
    auto *match = config.find_match("test@secure.com");
    EXPECT_NE(match, nullptr);
    EXPECT_EQ(match->sectionName, "pgp1");
}

TEST_F(ConfigTest, DuplicateStaticSectionThrows)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general", "", {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE}}, NodeType::SECTION},
         {"general", "", {{"milter_socket", "unix:/tmp/second.sock", {}, NodeType::VALUE}}, NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(configNode); }, std::invalid_argument);
}

TEST_F(ConfigTest, UnknownStaticSectionThrows)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general", "", {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE}}, NodeType::SECTION},
         {"unsupported", "", {{"some_key", "value", {}, NodeType::VALUE}}, NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(configNode); }, std::invalid_argument);
}

TEST_F(ConfigTest, UnknownDynamicSectionTypeThrows)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general", "", {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE}}, NodeType::SECTION},
         {"custom",
          "",
          {{"encryption_protocol", "madeup", {}, NodeType::VALUE}, {"match", ".*@example\\.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(configNode); }, std::invalid_argument);
}

TEST_F(ConfigTest, DynamicSectionTypeIsCaseInsensitive)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general", "", {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE}}, NodeType::SECTION},
         {"encrypt_pgp",
          "",
          {{"encryption_protocol", "PGP", {}, NodeType::VALUE},
           {"match", ".*@example\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    Config config = parse<Config>(configNode);
    ASSERT_EQ(config.encryptionSections.size(), 1u);
    EXPECT_EQ(config.encryptionSections[0]->encryption_protocol, EncryptionProtocol::Pgp);
}

TEST_F(ConfigTest, DynamicSectionsWithSameNameAsTypeAreHandledCorrectly)
{
    ConfigNode configNode{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://localhost", {}, NodeType::VALUE},
           {"signing_key", "/path/to/key", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pgp", // Section name equals type name "pgp"
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@pgp\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"smime", // Section name equals type name "smime"
          "",
          {{"encryption_protocol", "smime", {}, NodeType::VALUE},
           {"match", ".*@smime\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pdf", // Section name equals type name "pdf"
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE},
           {"match", ".*@pdf\\.com", {}, NodeType::VALUE},
           {"pdf_font_size", "12.0", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"none", // Section name equals type name "none"
          "",
          {{"encryption_protocol", "none", {}, NodeType::VALUE}, {"match", ".*@none\\.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    Config config = parse<Config>(configNode);

    // Verify that all 4 dynamic sections were correctly deserialized
    EXPECT_EQ(config.encryptionSections.size(), 4);

    // Verify each section has correct protocol and section name
    EXPECT_EQ(config.encryptionSections[0]->encryption_protocol, EncryptionProtocol::Pgp);
    EXPECT_EQ(config.encryptionSections[0]->sectionName, "pgp");

    EXPECT_EQ(config.encryptionSections[1]->encryption_protocol, EncryptionProtocol::Smime);
    EXPECT_EQ(config.encryptionSections[1]->sectionName, "smime");

    EXPECT_EQ(config.encryptionSections[2]->encryption_protocol, EncryptionProtocol::Pdf);
    EXPECT_EQ(config.encryptionSections[2]->sectionName, "pdf");

    EXPECT_EQ(config.encryptionSections[3]->encryption_protocol, EncryptionProtocol::None);
    EXPECT_EQ(config.encryptionSections[3]->sectionName, "none");

    // Verify that find_match works correctly
    auto *pgpMatch = config.find_match("test@pgp.com");
    EXPECT_NE(pgpMatch, nullptr);
    EXPECT_EQ(pgpMatch->encryption_protocol, EncryptionProtocol::Pgp);

    auto *smimeMatch = config.find_match("test@smime.com");
    EXPECT_NE(smimeMatch, nullptr);
    EXPECT_EQ(smimeMatch->encryption_protocol, EncryptionProtocol::Smime);
}

TEST_F(ConfigValidationTest, PgpSectionRejectsInvalidKeyPolicy)
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
                                {"key_not_found_policy", "invalid", {}, NodeType::VALUE}},
                               NodeType::SECTION}},
                             NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidPolicy); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, PgpSectionRequiresKeyPolicy)
{
    ConfigNode missingPolicy{"config",
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
                                {"match", ".*@test\\.com", {}, NodeType::VALUE}},
                               NodeType::SECTION}},
                             NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(missingPolicy); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, SmimeSectionRejectsRetrievePolicy)
{
    ConfigNode invalidSmime{"config",
                            "",
                            {{"general",
                              "",
                              {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                               {"log_type", "console", {}, NodeType::VALUE},
                               {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                              NodeType::SECTION},
                             {"encrypt_smime",
                              "",
                              {{"encryption_protocol", "smime", {}, NodeType::VALUE},
                               {"match", ".*@secure\\.com", {}, NodeType::VALUE},
                               {"key_not_found_policy", "retrieve", {}, NodeType::VALUE}},
                              NodeType::SECTION}},
                            NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidSmime); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, SmimeSectionRequiresKeyPolicy)
{
    ConfigNode missingPolicy{"config",
                             "",
                             {{"general",
                               "",
                               {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                                {"log_type", "console", {}, NodeType::VALUE},
                                {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                               NodeType::SECTION},
                              {"encrypt_smime",
                               "",
                               {{"encryption_protocol", "smime", {}, NodeType::VALUE},
                                {"match", ".*@secure\\.com", {}, NodeType::VALUE}},
                               NodeType::SECTION}},
                             NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(missingPolicy); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, GeneralSectionRejectsInvalidLogPriority)
{
    ConfigNode invalidPriority{"config",
                               "",
                               {{"general",
                                 "",
                                 {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                                  {"log_type", "console", {}, NodeType::VALUE},
                                  {"log_priority", "verbose", {}, NodeType::VALUE}},
                                 NodeType::SECTION}},
                               NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidPriority); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, GeneralSectionRejectsInvalidLogFacility)
{
    ConfigNode invalidFacility{"config",
                               "",
                               {{"general",
                                 "",
                                 {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                                  {"log_type", "syslog", {}, NodeType::VALUE},
                                  {"log_facility", "invalid", {}, NodeType::VALUE}},
                                 NodeType::SECTION}},
                               NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(invalidFacility); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, MissingEncryptionProtocolThrowsException)
{
    ConfigNode missingProtocol{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"encrypt_section", "", {{"match", ".*@test\\.com", {}, NodeType::VALUE}}, NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(missingProtocol); }, std::invalid_argument);
}

TEST_F(ConfigValidationTest, ConfigWithOnlyGeneralSectionWorks)
{
    // Edge case: Config with no encryption sections at all
    ConfigNode onlyGeneral{"config",
                           "",
                           {{"general",
                             "",
                             {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                              {"log_type", "console", {}, NodeType::VALUE},
                              {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                             NodeType::SECTION}},
                           NodeType::ROOT};

    EXPECT_NO_THROW({ Config config = parse<Config>(onlyGeneral); });

    Config config = parse<Config>(onlyGeneral);
    EXPECT_EQ(config.encryptionSections.size(), 0);
    EXPECT_EQ(config.general.milter_socket, "unix:/tmp/test.sock");
}

TEST_F(ConfigValidationTest, SigningKeyRequiredForMultipleEncryptionSections)
{
    ConfigNode multipleWithoutSigningKey{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pgp_section",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@example.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pdf_section",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE}, {"match", ".*@test.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_THROW(static_cast<void>(parse<Config>(multipleWithoutSigningKey)), std::invalid_argument);
}

TEST_F(ConfigValidationTest, SmtpServerRequiredForMultipleEncryptionSections)
{
    ConfigNode multipleWithoutSmtp{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"signing_key", "/path/to/key", {}, NodeType::VALUE},
           {"smtp_server", "", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pgp_section",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE}, {"match", ".*@example.com", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pdf_section",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE}, {"match", ".*@test.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_THROW(static_cast<void>(parse<Config>(multipleWithoutSmtp)), std::invalid_argument);
}

TEST_F(ConfigValidationTest, CrossSectionValidationPassesWithBothFields)
{
    ConfigNode validMultiple{
        "config",
        "",
        {{"general",
          "",
          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
           {"log_type", "console", {}, NodeType::VALUE},
           {"smtp_server", "smtp://localhost", {}, NodeType::VALUE},
           {"signing_key", "/path/to/key", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pgp_section",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@example.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pdf_section",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE}, {"match", ".*@test.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_NO_THROW({ Config config = parse<Config>(validMultiple); });

    Config config = parse<Config>(validMultiple);
    EXPECT_EQ(config.encryptionSections.size(), 2);
    EXPECT_EQ(config.general.signing_key, "/path/to/key");
    EXPECT_EQ(config.general.smtp_server, "smtp://localhost");
}

TEST_F(ConfigValidationTest, SingleEncryptionSectionDoesNotRequireSigningKey)
{
    ConfigNode singleSection{"config",
                             "",
                             {{"general",
                               "",
                               {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                                {"log_type", "console", {}, NodeType::VALUE},
                                {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                               NodeType::SECTION},
                              {"pgp_section",
                               "",
                               {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
                                {"match", ".*@example.com", {}, NodeType::VALUE},
                                {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                               NodeType::SECTION}},
                             NodeType::ROOT};

    EXPECT_NO_THROW({ Config config = parse<Config>(singleSection); });

    Config config = parse<Config>(singleSection);
    EXPECT_EQ(config.encryptionSections.size(), 1);
    EXPECT_TRUE(config.general.signing_key.empty());
}

TEST_F(MandatorySectionTest, MissingGeneralSectionThrowsException)
{
    // Config with only dynamic sections but no general section
    ConfigNode missingGeneral{
        "config",
        "",
        {{"pgp_section",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE}, {"match", ".*@example.com", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    EXPECT_THROW({ Config config = parse<Config>(missingGeneral); }, std::invalid_argument);
}

TEST_F(MandatorySectionTest, RegistryCorrectlyIdentifiesGeneralAsMandatory)
{
    EXPECT_TRUE(StaticSectionRegistry::isMandatory("general"));

    auto mandatorySections = StaticSectionRegistry::getMandatorySections();
    EXPECT_FALSE(mandatorySections.empty());
    EXPECT_TRUE(std::find(mandatorySections.begin(), mandatorySections.end(), "general") != mandatorySections.end());
}
