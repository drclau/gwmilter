#include "ini_reader.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace cfg2;

class IniReaderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create temp directory for test files
        test_dir = std::filesystem::temp_directory_path() / "cfg2_ini_tests";
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override
    {
        // Clean up test files
        std::filesystem::remove_all(test_dir);
    }

    void writeTestFile(const std::string &filename, const std::string &content)
    {
        std::ofstream file(test_dir / filename);
        file << content;
        file.close();
    }

    std::string getTestFilePath(const std::string &filename) { return (test_dir / filename).string(); }

    std::filesystem::path test_dir;
};

TEST_F(IniReaderTest, ParsesBasicIniFile)
{
    writeTestFile("basic.ini", R"([general]
key1 = value1
key2 = value2

[section1]
option1 = test
option2 = 42
)");

    ConfigNode root = parseIniFile(getTestFilePath("basic.ini"));

    EXPECT_EQ(root.key, "config");
    EXPECT_EQ(root.value, "");
    EXPECT_EQ(root.children.size(), 2);
    EXPECT_TRUE(root.isRoot());
    EXPECT_TRUE(root.isContainer());

    // Check general section
    const ConfigNode *general = root.findChild("general");
    ASSERT_NE(general, nullptr);
    EXPECT_EQ(general->children.size(), 2);
    EXPECT_TRUE(general->isSection());
    EXPECT_TRUE(general->isContainer());

    const ConfigNode *key1 = general->findChild("key1");
    ASSERT_NE(key1, nullptr);
    EXPECT_EQ(key1->value, "value1");
    EXPECT_TRUE(key1->isValue());
    EXPECT_FALSE(key1->isContainer());

    const ConfigNode *key2 = general->findChild("key2");
    ASSERT_NE(key2, nullptr);
    EXPECT_EQ(key2->value, "value2");
    EXPECT_TRUE(key2->isValue());

    // Check section1
    const ConfigNode *section1 = root.findChild("section1");
    ASSERT_NE(section1, nullptr);
    EXPECT_EQ(section1->children.size(), 2);
    EXPECT_TRUE(section1->isSection());

    const ConfigNode *option1 = section1->findChild("option1");
    ASSERT_NE(option1, nullptr);
    EXPECT_EQ(option1->value, "test");
    EXPECT_TRUE(option1->isValue());

    const ConfigNode *option2 = section1->findChild("option2");
    ASSERT_NE(option2, nullptr);
    EXPECT_EQ(option2->value, "42");
    EXPECT_TRUE(option2->isValue());
}

TEST_F(IniReaderTest, ParsesEmptySections)
{
    writeTestFile("empty.ini", R"([empty_section]

[another_section]
key = value
)");

    ConfigNode root = parseIniFile(getTestFilePath("empty.ini"));

    EXPECT_EQ(root.children.size(), 2);

    const ConfigNode *empty = root.findChild("empty_section");
    ASSERT_NE(empty, nullptr);
    EXPECT_EQ(empty->children.size(), 0);

    const ConfigNode *another = root.findChild("another_section");
    ASSERT_NE(another, nullptr);
    EXPECT_EQ(another->children.size(), 1);
}

TEST_F(IniReaderTest, ParsesCommaSeparatedValues)
{
    writeTestFile("comma.ini", R"([test]
match = pattern1,pattern2, pattern3
single = just_one
)");

    ConfigNode root = parseIniFile(getTestFilePath("comma.ini"));

    const ConfigNode *test = root.findChild("test");
    ASSERT_NE(test, nullptr);

    const ConfigNode *match = test->findChild("match");
    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->value, "pattern1,pattern2, pattern3");

    const ConfigNode *single = test->findChild("single");
    ASSERT_NE(single, nullptr);
    EXPECT_EQ(single->value, "just_one");
}

TEST_F(IniReaderTest, ParsesSpecialCharacters)
{
    writeTestFile("special.ini", R"([section]
regex_key = .*@example\.com
path_key = /path/with spaces/file.txt
special = !@#$%^&*()
)");

    ConfigNode root = parseIniFile(getTestFilePath("special.ini"));

    const ConfigNode *section = root.findChild("section");
    ASSERT_NE(section, nullptr);

    const ConfigNode *regex = section->findChild("regex_key");
    ASSERT_NE(regex, nullptr);
    EXPECT_EQ(regex->value, ".*@example\\.com");

    const ConfigNode *path = section->findChild("path_key");
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->value, "/path/with spaces/file.txt");

    const ConfigNode *special = section->findChild("special");
    ASSERT_NE(special, nullptr);
    EXPECT_EQ(special->value, "!@#$%^&*()");
}

TEST_F(IniReaderTest, ParsesComments)
{
    writeTestFile("comments.ini", R"(# This is a comment
[section1]
key1 = value1  ; inline comment
# Another comment
key2 = value2

; Semicolon comment style
[section2]
key3 = value3
)");

    ConfigNode root = parseIniFile(getTestFilePath("comments.ini"));

    EXPECT_EQ(root.children.size(), 2);

    const ConfigNode *section1 = root.findChild("section1");
    ASSERT_NE(section1, nullptr);
    EXPECT_EQ(section1->children.size(), 2);

    const ConfigNode *key1 = section1->findChild("key1");
    ASSERT_NE(key1, nullptr);
    EXPECT_EQ(key1->value, "value1  ; inline comment"); // SimpleIni includes inline comments

    const ConfigNode *section2 = root.findChild("section2");
    ASSERT_NE(section2, nullptr);
    EXPECT_EQ(section2->children.size(), 1);
}

TEST_F(IniReaderTest, ThrowsOnMissingFile)
{
    EXPECT_THROW(parseIniFile("/nonexistent/file.ini"), std::runtime_error);
}

TEST_F(IniReaderTest, HandlesMalformedFileGracefully)
{
    writeTestFile("malformed.ini", R"([section]
key = value
incomplete section
)");

    // SimpleIni is very lenient and will parse what it can
    ConfigNode root = parseIniFile(getTestFilePath("malformed.ini"));

    // Should have parsed the valid section with proper closing bracket
    const ConfigNode *section = root.findChild("section");
    ASSERT_NE(section, nullptr);

    const ConfigNode *key = section->findChild("key");
    ASSERT_NE(key, nullptr);
    EXPECT_EQ(key->value, "value");
}

TEST_F(IniReaderTest, ParsesRealConfigFile)
{
    // Load canonical test config from the repository
    const std::filesystem::path testdata_path =
        std::filesystem::path(__FILE__).parent_path() / "testdata" / "config.ini";
    ConfigNode root = parseIniFile(testdata_path.string());

    // Expect all sections from testdata/config.ini
    EXPECT_GE(root.children.size(), 5);

    // Verify general section
    const ConfigNode *general = root.findChild("general");
    ASSERT_NE(general, nullptr);
    EXPECT_NE(general->findChild("milter_socket"), nullptr);
    EXPECT_NE(general->findChild("smtp_server"), nullptr);
    EXPECT_NE(general->findChild("log_type"), nullptr);
    EXPECT_NE(general->findChild("log_priority"), nullptr);

    // Verify pgp section
    const ConfigNode *pgp = root.findChild("pgp");
    ASSERT_NE(pgp, nullptr);
    const ConfigNode *pgp_match = pgp->findChild("match");
    ASSERT_NE(pgp_match, nullptr);
    const ConfigNode *pgp_proto = pgp->findChild("encryption_protocol");
    ASSERT_NE(pgp_proto, nullptr);
    EXPECT_EQ(pgp_proto->value, "pgp");

    // Verify smime section
    const ConfigNode *smime = root.findChild("smime");
    ASSERT_NE(smime, nullptr);
    const ConfigNode *smime_proto = smime->findChild("encryption_protocol");
    ASSERT_NE(smime_proto, nullptr);
    EXPECT_EQ(smime_proto->value, "smime");

    // Verify pdf section
    const ConfigNode *pdf = root.findChild("pdf");
    ASSERT_NE(pdf, nullptr);
    const ConfigNode *pdf_proto = pdf->findChild("encryption_protocol");
    ASSERT_NE(pdf_proto, nullptr);
    EXPECT_EQ(pdf_proto->value, "pdf");

    // Verify none section
    const ConfigNode *none = root.findChild("none");
    ASSERT_NE(none, nullptr);
    const ConfigNode *none_proto = none->findChild("encryption_protocol");
    ASSERT_NE(none_proto, nullptr);
    EXPECT_EQ(none_proto->value, "none");
}

TEST_F(IniReaderTest, HandlesEmptyValues)
{
    writeTestFile("empty_values.ini", R"([section]
empty_key = 
another_key = value
also_empty =
)");

    ConfigNode root = parseIniFile(getTestFilePath("empty_values.ini"));

    const ConfigNode *section = root.findChild("section");
    ASSERT_NE(section, nullptr);
    EXPECT_EQ(section->children.size(), 3);

    const ConfigNode *empty = section->findChild("empty_key");
    ASSERT_NE(empty, nullptr);
    EXPECT_EQ(empty->value, "");

    const ConfigNode *another = section->findChild("another_key");
    ASSERT_NE(another, nullptr);
    EXPECT_EQ(another->value, "value");

    const ConfigNode *also_empty = section->findChild("also_empty");
    ASSERT_NE(also_empty, nullptr);
    EXPECT_EQ(also_empty->value, "");
}

TEST_F(IniReaderTest, NodeTypeValidation)
{
    writeTestFile("types.ini", R"([section]
key = value
)");

    ConfigNode root = parseIniFile(getTestFilePath("types.ini"));

    // Root should be ROOT type
    EXPECT_TRUE(root.isRoot());
    EXPECT_FALSE(root.isSection());
    EXPECT_FALSE(root.isValue());

    // Section should be SECTION type
    const ConfigNode *section = root.findChild("section");
    ASSERT_NE(section, nullptr);
    EXPECT_FALSE(section->isRoot());
    EXPECT_TRUE(section->isSection());
    EXPECT_FALSE(section->isValue());

    // Key should be VALUE type
    const ConfigNode *key = section->findChild("key");
    ASSERT_NE(key, nullptr);
    EXPECT_FALSE(key->isRoot());
    EXPECT_FALSE(key->isSection());
    EXPECT_TRUE(key->isValue());

    // Test type validation error - trying to find child in VALUE node should throw
    EXPECT_THROW(key->findChild("nonexistent"), std::logic_error);
}

TEST_F(IniReaderTest, PreservesSectionOrder)
{
    writeTestFile("order.ini", R"([general]
milter_socket = unix:/tmp/test.sock
smtp_server = smtp://localhost

[third_section]
encryption_protocol = none
match = .*@third\.com

[first_section] 
encryption_protocol = pgp
match = .*@first\.com

[second_section]
encryption_protocol = pdf
match = .*@second\.com

[fourth_section]
encryption_protocol = smime
match = .*@fourth\.com
)");

    ConfigNode root = parseIniFile(getTestFilePath("order.ini"));

    // Verify we have all 5 sections
    EXPECT_EQ(root.children.size(), 5);

    // Verify sections appear in file order, not alphabetical order
    EXPECT_EQ(root.children[0].key, "general");
    EXPECT_EQ(root.children[1].key, "third_section");
    EXPECT_EQ(root.children[2].key, "first_section");
    EXPECT_EQ(root.children[3].key, "second_section");
    EXPECT_EQ(root.children[4].key, "fourth_section");

    // Also verify each section has the expected protocol to ensure content is correct
    EXPECT_EQ(root.children[1].findChild("encryption_protocol")->value, "none");
    EXPECT_EQ(root.children[2].findChild("encryption_protocol")->value, "pgp");
    EXPECT_EQ(root.children[3].findChild("encryption_protocol")->value, "pdf");
    EXPECT_EQ(root.children[4].findChild("encryption_protocol")->value, "smime");
}