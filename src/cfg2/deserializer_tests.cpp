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

TEST_F(DeserializerTest, DeserializeSmimeEncryptionSection)
{
    ConfigNode node{"smime_section",
                    "",
                    {{"match", ".*@secure\\.com,admin@.*", {}, NodeType::VALUE},
                     {"encryption_protocol", "smime", {}, NodeType::VALUE},
                     {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    SmimeEncryptionSection result = deserialize<SmimeEncryptionSection>(node);

    EXPECT_EQ(result.match.size(), 2);
    EXPECT_EQ(result.match[0], ".*@secure\\.com");
    EXPECT_EQ(result.match[1], "admin@.*");
    EXPECT_EQ(result.encryption_protocol, "smime");
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

// Vector deserialization tests
// NOTE: this functionality is not used with INI files, but is available for future expansion to other formats
TEST_F(DeserializerTest, DeserializeVectorFromChildNodes)
{
    ConfigNode node{"section",
                    "",
                    {{"match",
                      "",
                      {{"pattern1", ".*@first\\.com", {}, NodeType::VALUE},
                       {"pattern2", ".*@second\\.org", {}, NodeType::VALUE},
                       {"pattern3", ".*@third\\.net", {}, NodeType::VALUE}},
                      NodeType::SECTION},
                     {"encryption_protocol", "pgp", {}, NodeType::VALUE},
                     {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    PgpEncryptionSection result = deserialize<PgpEncryptionSection>(node);

    EXPECT_EQ(result.match.size(), 3);
    EXPECT_EQ(result.match[0], ".*@first\\.com");
    EXPECT_EQ(result.match[1], ".*@second\\.org");
    EXPECT_EQ(result.match[2], ".*@third\\.net");
}

TEST_F(DeserializerTest, DeserializeEmptyVector)
{
    // Test with NoneEncryptionSection which doesn't require validation like PgpEncryptionSection
    ConfigNode node{"section",
                    "",
                    {{"match", "", {}, NodeType::VALUE}, {"encryption_protocol", "none", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    // Empty match patterns should fail validation for any encryption section
    EXPECT_THROW({ NoneEncryptionSection result = deserialize<NoneEncryptionSection>(node); }, std::invalid_argument);
}

TEST_F(DeserializerTest, DeserializeVectorWithWhitespace)
{
    ConfigNode node{"section",
                    "",
                    {{"match", "  .*@test\\.com  , \t.*@example\\.org\t,  ", {}, NodeType::VALUE},
                     {"encryption_protocol", "pgp", {}, NodeType::VALUE},
                     {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    PgpEncryptionSection result = deserialize<PgpEncryptionSection>(node);

    EXPECT_EQ(result.match.size(), 2);
    EXPECT_EQ(result.match[0], ".*@test\\.com");
    EXPECT_EQ(result.match[1], ".*@example\\.org");
}

TEST_F(DeserializerTest, DeserializeSingleElementVector)
{
    ConfigNode node{"section",
                    "",
                    {{"match", ".*@single\\.com", {}, NodeType::VALUE},
                     {"encryption_protocol", "pgp", {}, NodeType::VALUE},
                     {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    PgpEncryptionSection result = deserialize<PgpEncryptionSection>(node);

    EXPECT_EQ(result.match.size(), 1);
    EXPECT_EQ(result.match[0], ".*@single\\.com");
}

TEST_F(DeserializerTest, ValidationExceptionsPropagateFromDeserialization)
{
    // Test with GeneralSection that should fail validation
    ConfigNode invalidNode{"general",
                           "",
                           {{"milter_socket", "", {}, NodeType::VALUE}, // Empty socket should fail
                            {"log_type", "console", {}, NodeType::VALUE},
                            {"smtp_server", "smtp://localhost", {}, NodeType::VALUE}},
                           NodeType::SECTION};

    EXPECT_THROW({ GeneralSection result = deserialize<GeneralSection>(invalidNode); }, std::invalid_argument);
}

TEST_F(DeserializerTest, ValidationWorksWithEncryptionSections)
{
    // Test PgpEncryptionSection validation
    ConfigNode validPgp{"pgp_section",
                        "",
                        {{"match", ".*@test\\.com", {}, NodeType::VALUE},
                         {"encryption_protocol", "pgp", {}, NodeType::VALUE},
                         {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                        NodeType::SECTION};

    EXPECT_NO_THROW({ PgpEncryptionSection result = deserialize<PgpEncryptionSection>(validPgp); });

    // Test invalid protocol
    ConfigNode invalidPgp{"pgp_section",
                          "",
                          {{"match", ".*@test\\.com", {}, NodeType::VALUE},
                           {"encryption_protocol", "invalid", {}, NodeType::VALUE},
                           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
                          NodeType::SECTION};

    EXPECT_THROW(
        { PgpEncryptionSection result = deserialize<PgpEncryptionSection>(invalidPgp); }, std::invalid_argument);
}

class FromStringTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

// fromString<T>() function tests
TEST_F(FromStringTest, BoolConversionTrueFalse)
{
    EXPECT_EQ(fromString<bool>("true"), true);
    EXPECT_EQ(fromString<bool>("false"), false);
    EXPECT_EQ(fromString<bool>("TRUE"), true);
    EXPECT_EQ(fromString<bool>("FALSE"), false);
    EXPECT_EQ(fromString<bool>("True"), true);
    EXPECT_EQ(fromString<bool>("False"), false);
}

TEST_F(FromStringTest, BoolConversionOneZero)
{
    EXPECT_EQ(fromString<bool>("1"), true);
    EXPECT_EQ(fromString<bool>("0"), false);
}

TEST_F(FromStringTest, BoolConversionYesNo)
{
    EXPECT_EQ(fromString<bool>("yes"), true);
    EXPECT_EQ(fromString<bool>("no"), false);
    EXPECT_EQ(fromString<bool>("YES"), true);
    EXPECT_EQ(fromString<bool>("NO"), false);
    EXPECT_EQ(fromString<bool>("Yes"), true);
    EXPECT_EQ(fromString<bool>("No"), false);
}

TEST_F(FromStringTest, BoolConversionOnOff)
{
    EXPECT_EQ(fromString<bool>("on"), true);
    EXPECT_EQ(fromString<bool>("off"), false);
    EXPECT_EQ(fromString<bool>("ON"), true);
    EXPECT_EQ(fromString<bool>("OFF"), false);
    EXPECT_EQ(fromString<bool>("On"), true);
    EXPECT_EQ(fromString<bool>("Off"), false);
}

TEST_F(FromStringTest, BoolConversionInvalidValues)
{
    EXPECT_THROW(fromString<bool>("invalid"), std::runtime_error);
    EXPECT_THROW(fromString<bool>("2"), std::runtime_error);
    EXPECT_THROW(fromString<bool>(""), std::runtime_error);
}

TEST_F(FromStringTest, NumericConversions)
{
    EXPECT_EQ(fromString<int>("42"), 42);
    EXPECT_EQ(fromString<int>("-17"), -17);
    EXPECT_FLOAT_EQ(fromString<float>("3.14"), 3.14f);
    EXPECT_DOUBLE_EQ(fromString<double>("2.71828"), 2.71828);
}

TEST_F(FromStringTest, NumericConversionFailures)
{
    EXPECT_THROW(fromString<int>("not_a_number"), std::runtime_error);
    EXPECT_THROW(fromString<int>("abc123"), std::runtime_error); // Text before number
    EXPECT_THROW(fromString<float>("invalid_float"), std::runtime_error);
    EXPECT_THROW(fromString<double>("not.a.double"), std::runtime_error);
}

TEST_F(FromStringTest, StringConversion)
{
    EXPECT_EQ(fromString<std::string>("hello world"), "hello world");
    EXPECT_EQ(fromString<std::string>(""), "");
    EXPECT_EQ(fromString<std::string>("123"), "123");
}

class DeserializerErrorTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

// Error handling tests
TEST_F(DeserializerErrorTest, TypeConversionFailuresInDeserialization)
{
    ConfigNode badIntNode{"general",
                          "",
                          {{"milter_socket", "unix:/tmp/test.sock", {}, NodeType::VALUE},
                           {"log_type", "console", {}, NodeType::VALUE},
                           {"smtp_server", "smtp://localhost", {}, NodeType::VALUE},
                           {"milter_timeout", "not_a_number", {}, NodeType::VALUE}},
                          NodeType::SECTION};

    EXPECT_THROW({ GeneralSection result = deserialize<GeneralSection>(badIntNode); }, std::runtime_error);
}

TEST_F(DeserializerErrorTest, TypeConversionFailuresInPdfSection)
{
    ConfigNode badFloatNode{"pdf_section",
                            "",
                            {{"match", ".*@test\\.com", {}, NodeType::VALUE},
                             {"encryption_protocol", "pdf", {}, NodeType::VALUE},
                             {"pdf_font_size", "invalid_float", {}, NodeType::VALUE}},
                            NodeType::SECTION};

    EXPECT_THROW(
        { PdfEncryptionSection result = deserialize<PdfEncryptionSection>(badFloatNode); }, std::runtime_error);
}

TEST_F(DeserializerErrorTest, MalformedConfigNodeHandling)
{
    // Test findChild() error for non-container nodes
    ConfigNode leafNode{"value", "some_value", {}, NodeType::VALUE};

    EXPECT_THROW(leafNode.findChild("nonexistent"), std::logic_error);
}

class DeserializerAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

// Advanced features tests
TEST_F(DeserializerAdvancedTest, MakeDeserializerFunctionWorks)
{
    // Test the make_deserializer template helper function
    auto deserializer = make_deserializer<GeneralSection>(field("milter_socket", &GeneralSection::milter_socket),
                                                          field("log_type", &GeneralSection::log_type),
                                                          field("smtp_server", &GeneralSection::smtp_server));

    ConfigNode node{"general",
                    "",
                    {{"milter_socket", "unix:/tmp/advanced.sock", {}, NodeType::VALUE},
                     {"log_type", "syslog", {}, NodeType::VALUE},
                     {"smtp_server", "smtp://advanced.test.com", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    GeneralSection result = deserializer(node);

    EXPECT_EQ(result.milter_socket, "unix:/tmp/advanced.sock");
    EXPECT_EQ(result.log_type, "syslog");
    EXPECT_EQ(result.smtp_server, "smtp://advanced.test.com");
    // Fields not specified in deserializer should have default values
    EXPECT_EQ(result.daemonize, false);
    EXPECT_EQ(result.milter_timeout, -1);
}
