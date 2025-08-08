#include "ini_config.hpp"

namespace cfg2 {

// Register static section
REGISTER_STRUCT(GeneralSection,
    field("appName", &GeneralSection::appName),
    field("version", &GeneralSection::version),
    field("logLevel", &GeneralSection::logLevel)
)

// Register dynamic section types
REGISTER_DYNAMIC_SECTION(DatabaseSection, "database",
    field("match", &DatabaseSection::match),
    field("host", &DatabaseSection::host),
    field("port", &DatabaseSection::port),
    field("username", &DatabaseSection::username),
    field("password", &DatabaseSection::password)
)

REGISTER_DYNAMIC_SECTION(CacheSection, "cache",
    field("match", &CacheSection::match),
    field("redisHost", &CacheSection::redisHost),
    field("redisPort", &CacheSection::redisPort),
    field("ttl", &CacheSection::ttl)
)

REGISTER_DYNAMIC_SECTION(ServiceSection, "service",
    field("match", &ServiceSection::match),
    field("endpoint", &ServiceSection::endpoint),
    field("timeout", &ServiceSection::timeout),
    field("enabled", &ServiceSection::enabled)
)

// Implementation of custom deserializer for IniConfig
template<>
IniConfig deserialize<IniConfig>(const TreeNode& node) {
    IniConfig config{};
    
    // Deserialize static general section
    const TreeNode* generalNode = node.findChild("general");
    if (generalNode) {
        config.general = deserialize<GeneralSection>(*generalNode);
    }
    
    // Deserialize dynamic sections in order
    for (const auto& child : node.children) {
        if (child.key != "general") {
            const TreeNode* typeNode = child.findChild("type");
            if (typeNode && DynamicSectionRegistry::hasType(typeNode->value)) {
                config.dynamicSections.push_back(
                    DynamicSectionRegistry::create(typeNode->value, child)
                );
            }
        }
    }
    
    return config;
}

} // namespace cfg2