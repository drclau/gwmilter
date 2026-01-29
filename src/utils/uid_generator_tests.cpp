#include "uid_generator.hpp"
#include <cctype>
#include <gtest/gtest.h>
#include <set>

using namespace gwmilter;

class UidGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(UidGeneratorTest, GenerateReturnsExpectedLength)
{
    uid_generator gen;
    std::string uid = gen.generate();

    // Format is {:08X} which produces 8 hex characters
    EXPECT_EQ(uid.length(), 8);
}

TEST_F(UidGeneratorTest, GenerateReturnsHexCharactersOnly)
{
    uid_generator gen;
    std::string uid = gen.generate();

    // Check all characters are valid hex (0-9, A-F)
    for (char c: uid) {
        EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(c)));
        // fmt's {:X} format produces uppercase hex
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'));
    }
}

TEST_F(UidGeneratorTest, GenerateReturnsUniqueValues)
{
    uid_generator gen;
    std::set<std::string> generated_uids;

    // Generate 1000 UIDs and check they're unique
    // With 32-bit random values formatted as 8 hex digits,
    // the chance of collision in 1000 samples is extremely low
    for (int i = 0; i < 1000; ++i) {
        std::string uid = gen.generate();
        EXPECT_TRUE(generated_uids.insert(uid).second) << "Duplicate UID generated: " << uid;
    }
}
