#include "core.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace cfg2;

class MultipleSameTypeTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(MultipleSameTypeTest, MultipleSectionsWithSameTypeAreSupported)
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
         {"pgp_internal",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@internal\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "retrieve", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pgp_external",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@external\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pgp_partners",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@partners\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "discard", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    // This should work without throwing an exception
    Config config = parse<Config>(configNode);

    // Verify that all 3 PGP sections were correctly created
    EXPECT_EQ(config.encryptionSections.size(), 3);

    // Verify that all sections have the same type but different names and configurations
    EXPECT_EQ(config.encryptionSections[0]->encryption_protocol, "pgp");
    EXPECT_EQ(config.encryptionSections[0]->sectionName, "pgp_internal");
    EXPECT_EQ(config.encryptionSections[0]->type, "pgp");

    EXPECT_EQ(config.encryptionSections[1]->encryption_protocol, "pgp");
    EXPECT_EQ(config.encryptionSections[1]->sectionName, "pgp_external");
    EXPECT_EQ(config.encryptionSections[1]->type, "pgp");

    EXPECT_EQ(config.encryptionSections[2]->encryption_protocol, "pgp");
    EXPECT_EQ(config.encryptionSections[2]->sectionName, "pgp_partners");
    EXPECT_EQ(config.encryptionSections[2]->type, "pgp");

    // Verify that each section has different matching patterns and policies
    auto *pgpInternal = dynamic_cast<PgpEncryptionSection *>(config.encryptionSections[0].get());
    auto *pgpExternal = dynamic_cast<PgpEncryptionSection *>(config.encryptionSections[1].get());
    auto *pgpPartners = dynamic_cast<PgpEncryptionSection *>(config.encryptionSections[2].get());

    ASSERT_NE(pgpInternal, nullptr);
    ASSERT_NE(pgpExternal, nullptr);
    ASSERT_NE(pgpPartners, nullptr);

    // Test matching functionality for each section
    EXPECT_TRUE(pgpInternal->matches("user@internal.com"));
    EXPECT_FALSE(pgpInternal->matches("user@external.com"));

    EXPECT_TRUE(pgpExternal->matches("user@external.com"));
    EXPECT_FALSE(pgpExternal->matches("user@internal.com"));

    EXPECT_TRUE(pgpPartners->matches("user@partners.com"));
    EXPECT_FALSE(pgpPartners->matches("user@internal.com"));

    // Test find_match returns the correct section
    EXPECT_EQ(config.find_match("user@internal.com"), pgpInternal);
    EXPECT_EQ(config.find_match("user@external.com"), pgpExternal);
    EXPECT_EQ(config.find_match("user@partners.com"), pgpPartners);
    EXPECT_EQ(config.find_match("user@unknown.com"), nullptr);
}

TEST_F(MultipleSameTypeTest, MultipleSectionsWithDifferentTypesAlsoWork)
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
         {"pgp_section1",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@pgp1\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "retrieve", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pgp_section2",
          "",
          {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
           {"match", ".*@pgp2\\.com", {}, NodeType::VALUE},
           {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pdf_section1",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE},
           {"match", ".*@pdf1\\.com", {}, NodeType::VALUE},
           {"pdf_font_size", "10.0", {}, NodeType::VALUE}},
          NodeType::SECTION},
         {"pdf_section2",
          "",
          {{"encryption_protocol", "pdf", {}, NodeType::VALUE},
           {"match", ".*@pdf2\\.com", {}, NodeType::VALUE},
           {"pdf_font_size", "14.0", {}, NodeType::VALUE}},
          NodeType::SECTION}},
        NodeType::ROOT};

    Config config = parse<Config>(configNode);

    // Verify that all 4 sections were correctly created (2 PGP + 2 PDF)
    EXPECT_EQ(config.encryptionSections.size(), 4);

    // Test that each section works correctly
    EXPECT_EQ(config.find_match("user@pgp1.com")->encryption_protocol, "pgp");
    EXPECT_EQ(config.find_match("user@pgp2.com")->encryption_protocol, "pgp");
    EXPECT_EQ(config.find_match("user@pdf1.com")->encryption_protocol, "pdf");
    EXPECT_EQ(config.find_match("user@pdf2.com")->encryption_protocol, "pdf");

    // Verify different section configurations
    auto *pgpSection1 = dynamic_cast<PgpEncryptionSection *>(config.encryptionSections[0].get());
    auto *pgpSection2 = dynamic_cast<PgpEncryptionSection *>(config.encryptionSections[1].get());
    auto *pdfSection1 = dynamic_cast<PdfEncryptionSection *>(config.encryptionSections[2].get());
    auto *pdfSection2 = dynamic_cast<PdfEncryptionSection *>(config.encryptionSections[3].get());

    EXPECT_EQ(pgpSection1->key_not_found_policy, "retrieve");
    EXPECT_EQ(pgpSection2->key_not_found_policy, "reject");
    EXPECT_FLOAT_EQ(pdfSection1->pdf_font_size, 10.0f);
    EXPECT_FLOAT_EQ(pdfSection2->pdf_font_size, 14.0f);
}
