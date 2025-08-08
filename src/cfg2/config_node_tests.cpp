#include "config_node.hpp"
#include <gtest/gtest.h>

using namespace cfg2;

class ConfigNodeTest : public ::testing::Test {
protected:
    void SetUp() override { }
    void TearDown() override { }
};

TEST_F(ConfigNodeTest, BasicConstructorWorks)
{
    ConfigNode node{"key", "value"};

    EXPECT_EQ(node.key, "key");
    EXPECT_EQ(node.value, "value");
    EXPECT_TRUE(node.children.empty());
}

TEST_F(ConfigNodeTest, ConstructorWithChildrenWorks)
{
    ConfigNode node{"parent",
                    "parent_value",
                    {{"child1", "value1", {}, NodeType::VALUE}, {"child2", "value2", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    EXPECT_EQ(node.key, "parent");
    EXPECT_EQ(node.value, "parent_value");
    EXPECT_EQ(node.children.size(), 2);
    EXPECT_EQ(node.children[0].key, "child1");
    EXPECT_EQ(node.children[0].value, "value1");
    EXPECT_EQ(node.children[1].key, "child2");
    EXPECT_EQ(node.children[1].value, "value2");
}

TEST_F(ConfigNodeTest, FindChildFindsExistingChild)
{
    ConfigNode node{"parent",
                    "",
                    {{"child1", "value1", {}, NodeType::VALUE},
                     {"target", "target_value", {}, NodeType::VALUE},
                     {"child3", "value3", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    const ConfigNode *found = node.findChild("target");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->key, "target");
    EXPECT_EQ(found->value, "target_value");
}

TEST_F(ConfigNodeTest, FindChildReturnsNullForMissingChild)
{
    ConfigNode node{"parent",
                    "",
                    {{"child1", "value1", {}, NodeType::VALUE}, {"child2", "value2", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    const ConfigNode *found = node.findChild("missing");
    EXPECT_EQ(found, nullptr);
}

TEST_F(ConfigNodeTest, FindChildWorksOnEmptyNode)
{
    ConfigNode node{"empty", "", {}, NodeType::SECTION};

    const ConfigNode *found = node.findChild("anything");
    EXPECT_EQ(found, nullptr);
}

TEST_F(ConfigNodeTest, FindChildReturnsFirstMatch)
{
    ConfigNode node{"parent",
                    "",
                    {{"duplicate", "first", {}, NodeType::VALUE},
                     {"other", "other_value", {}, NodeType::VALUE},
                     {"duplicate", "second", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    const ConfigNode *found = node.findChild("duplicate");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->value, "first");
}

TEST_F(ConfigNodeTest, NestedStructureWorks)
{
    ConfigNode node{"root",
                    "",
                    {{"level1",
                      "",
                      {{"level2", "", {{"level3", "deep_value", {}, NodeType::VALUE}}, NodeType::SECTION}},
                      NodeType::SECTION},
                     {"another", "another_value", {}, NodeType::VALUE}},
                    NodeType::ROOT};

    EXPECT_EQ(node.children.size(), 2);

    const ConfigNode *level1 = node.findChild("level1");
    EXPECT_NE(level1, nullptr);

    const ConfigNode *level2 = level1->findChild("level2");
    EXPECT_NE(level2, nullptr);

    const ConfigNode *level3 = level2->findChild("level3");
    EXPECT_NE(level3, nullptr);
    EXPECT_EQ(level3->value, "deep_value");
}

TEST_F(ConfigNodeTest, EmptyKeyAndValueWork)
{
    ConfigNode node{"",
                    "",
                    {{"", "empty_key_value", {}, NodeType::VALUE}, {"normal_key", "", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    EXPECT_EQ(node.key, "");
    EXPECT_EQ(node.value, "");
    EXPECT_EQ(node.children.size(), 2);
    EXPECT_EQ(node.children[0].key, "");
    EXPECT_EQ(node.children[0].value, "empty_key_value");
    EXPECT_EQ(node.children[1].key, "normal_key");
    EXPECT_EQ(node.children[1].value, "");
}

TEST_F(ConfigNodeTest, FindChildWithEmptyKey)
{
    ConfigNode node{"parent",
                    "",
                    {{"", "empty_key_value", {}, NodeType::VALUE}, {"normal", "normal_value", {}, NodeType::VALUE}},
                    NodeType::SECTION};

    const ConfigNode *found = node.findChild("");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->value, "empty_key_value");
}