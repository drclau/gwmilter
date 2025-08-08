#pragma once

#include "dynamic_section.hpp"
#include <vector>

namespace cfg2 {

// ============= INI CONFIGURATION EXAMPLE =============

// Static general section - always present
struct GeneralSection {
    std::string appName;
    std::string version;
    int logLevel;
};

// Dynamic section types
struct DatabaseSection : BaseDynamicSection {
    std::string host;
    int port;
    std::string username;
    std::string password;
};

struct CacheSection : BaseDynamicSection {
    std::string redisHost;
    int redisPort;
    int ttl;
};

struct ServiceSection : BaseDynamicSection {
    std::string endpoint;
    int timeout;
    bool enabled;
};

// Main INI configuration
struct IniConfig {
    GeneralSection general;
    std::vector<std::unique_ptr<BaseDynamicSection>> dynamicSections;
    
    // Find first dynamic section that matches the given value
    BaseDynamicSection* find_match(const std::string& value) const {
        for (const auto& section : dynamicSections) {
            if (section->matches(value)) {
                return section.get();
            }
        }
        return nullptr;
    }
};

// Custom deserializer for IniConfig to handle mixed static/dynamic sections
template<>
struct is_deserializable_struct<IniConfig> : std::true_type {};

template<>
IniConfig deserialize<IniConfig>(const TreeNode& node);

} // namespace cfg2