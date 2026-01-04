#include "core.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace cfg2;

class DynamicSectionTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(DynamicSectionTest, BaseDynamicSectionMatchesRegex)
{
    BaseDynamicSection section;
    section.match = {".*@test\\.com", "admin@.*"};
    section.compileMatches();

    EXPECT_TRUE(section.matches("user@test.com"));
    EXPECT_TRUE(section.matches("admin@anywhere.org"));
    EXPECT_FALSE(section.matches("user@other.com"));
}

TEST_F(DynamicSectionTest, InvalidRegexThrowsException)
{
    BaseDynamicSection section;
    section.match = {"valid@test\\.com", "[invalid", "another@test\\.com"};

    // Should throw exception for invalid regex pattern
    EXPECT_THROW({ section.compileMatches(); }, std::invalid_argument);
}

TEST_F(DynamicSectionTest, ValidRegexPatternsCompileSuccessfully)
{
    BaseDynamicSection section;
    section.match = {".*@test\\.com", "admin@.*", "^user\\d+@domain\\.org$"};

    // Valid patterns should compile without throwing
    EXPECT_NO_THROW(section.compileMatches());

    EXPECT_TRUE(section.matches("user@test.com"));
    EXPECT_TRUE(section.matches("admin@anywhere.org"));
    EXPECT_TRUE(section.matches("user123@domain.org"));
    EXPECT_FALSE(section.matches("user@other.com"));
}

TEST_F(DynamicSectionTest, RegistryHasExpectedTypes)
{
    EXPECT_TRUE(DynamicSectionRegistry::hasType("pgp"));
    EXPECT_TRUE(DynamicSectionRegistry::hasType("smime"));
    EXPECT_TRUE(DynamicSectionRegistry::hasType("pdf"));
    EXPECT_TRUE(DynamicSectionRegistry::hasType("none"));
    EXPECT_FALSE(DynamicSectionRegistry::hasType("nonexistent_type"));
}

TEST_F(DynamicSectionTest, RegistryCanCreatePgpSection)
{
    ConfigNode node{"pgp_section",
                    "",
                    {{"match", ".*@secure\\.com", {}, NodeType::VALUE},
                     {"encryption_protocol", "pgp", {}, NodeType::VALUE},
                     {"key_not_found_policy", "retrieve", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    auto section = DynamicSectionRegistry::create("pgp", node);
    EXPECT_NE(section, nullptr);

    auto *pgpSection = dynamic_cast<PgpEncryptionSection *>(section.get());
    EXPECT_NE(pgpSection, nullptr);
    EXPECT_EQ(pgpSection->encryption_protocol, EncryptionProtocol::Pgp);
    EXPECT_EQ(pgpSection->key_not_found_policy, KeyNotFoundPolicy::Retrieve);
    EXPECT_EQ(pgpSection->sectionName, "pgp_section");
    EXPECT_EQ(pgpSection->type, "pgp");
}

TEST_F(DynamicSectionTest, RegistryCanCreatePdfSection)
{
    ConfigNode node{"pdf_section",
                    "",
                    {{"match", ".*@external\\.org", {}, NodeType::VALUE},
                     {"encryption_protocol", "pdf", {}, NodeType::VALUE},
                     {"pdf_font_size", "12.0", {}, NodeType::VALUE},
                     {"pdf_attachment", "secure.pdf", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    auto section = DynamicSectionRegistry::create("pdf", node);
    EXPECT_NE(section, nullptr);

    auto *pdfSection = dynamic_cast<PdfEncryptionSection *>(section.get());
    EXPECT_NE(pdfSection, nullptr);
    EXPECT_EQ(pdfSection->encryption_protocol, EncryptionProtocol::Pdf);
    EXPECT_FLOAT_EQ(pdfSection->pdf_font_size, 12.0f);
    EXPECT_EQ(pdfSection->pdf_attachment, "secure.pdf");
    EXPECT_EQ(pdfSection->sectionName, "pdf_section");
    EXPECT_EQ(pdfSection->type, "pdf");
}

TEST_F(DynamicSectionTest, RegistryCanCreateSmimeSection)
{
    ConfigNode node{"smime_section",
                    "",
                    {{"match", ".*@corporate\\.com", {}, NodeType::VALUE},
                     {"encryption_protocol", "smime", {}, NodeType::VALUE},
                     {"key_not_found_policy", "discard", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    auto section = DynamicSectionRegistry::create("smime", node);
    EXPECT_NE(section, nullptr);

    auto *smimeSection = dynamic_cast<SmimeEncryptionSection *>(section.get());
    EXPECT_NE(smimeSection, nullptr);
    EXPECT_EQ(smimeSection->encryption_protocol, EncryptionProtocol::Smime);
    EXPECT_EQ(smimeSection->key_not_found_policy, KeyNotFoundPolicy::Discard);
    EXPECT_EQ(smimeSection->sectionName, "smime_section");
    EXPECT_EQ(smimeSection->type, "smime");
}

TEST_F(DynamicSectionTest, RegistryCanCreateNoneSection)
{
    ConfigNode node{
        "none_section",
        "",
        {{"match", ".*@public\\.org", {}, NodeType::VALUE}, {"encryption_protocol", "none", {}, NodeType::VALUE}},
        NodeType::SECTION};

    auto section = DynamicSectionRegistry::create("none", node);
    EXPECT_NE(section, nullptr);

    auto *noneSection = dynamic_cast<NoneEncryptionSection *>(section.get());
    EXPECT_NE(noneSection, nullptr);
    EXPECT_EQ(noneSection->encryption_protocol, EncryptionProtocol::None);
    EXPECT_EQ(noneSection->sectionName, "none_section");
    EXPECT_EQ(noneSection->type, "none");
}

TEST_F(DynamicSectionTest, RegistryThrowsForUnknownType)
{
    ConfigNode node{"test", "", {}, NodeType::SECTION};

    EXPECT_THROW({ auto section = DynamicSectionRegistry::create("unknown_type", node); }, std::runtime_error);
}

TEST_F(DynamicSectionTest, RegistryThrowsForInvalidRegex)
{
    ConfigNode node{"invalid_regex_section",
                    "",
                    {{"match", "[invalid_regex", {}, NodeType::VALUE}, // Missing closing bracket
                     {"encryption_protocol", "none", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    EXPECT_THROW({ auto section = DynamicSectionRegistry::create("none", node); }, std::invalid_argument);
}

TEST_F(DynamicSectionTest, CreatedSectionCompilesMatches)
{
    ConfigNode node{"test_section",
                    "",
                    {{"match", ".*@example\\.com,admin@.*", {}, NodeType::VALUE},
                     {"encryption_protocol", "none", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    auto section = DynamicSectionRegistry::create("none", node);

    EXPECT_TRUE(section->matches("user@example.com"));
    EXPECT_TRUE(section->matches("admin@anywhere.org"));
    EXPECT_FALSE(section->matches("user@other.com"));
}