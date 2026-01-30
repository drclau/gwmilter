#include "body_handler.hpp"
#include <gtest/gtest.h>

using namespace gwmilter;

class NoopBodyHandlerTest : public ::testing::Test {
protected:
    noop_body_handler handler;
};

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
