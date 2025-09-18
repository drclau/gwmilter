#pragma once

#include <algorithm>
#include <cctype>
#include <fmt/core.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace cfg2 {

// Node type enumeration for runtime type checking
enum class NodeType {
    ROOT, // Top-level config node
    SECTION, // [section_name]
    VALUE // key = value
};

// Configuration node structure for hierarchical data
struct ConfigNode {
    std::string key;
    std::string value;
    std::vector<ConfigNode> children;
    NodeType type = NodeType::VALUE; // Default for leaf nodes

    // Helper to find child by key
    const ConfigNode *findChild(const std::string &childKey) const
    {
        if (!isContainer()) {
            throw std::logic_error(fmt::format("Cannot find child '{}' in non-container node '{}' (type: {})", childKey,
                                               key, (type == NodeType::VALUE ? "VALUE" : "UNKNOWN")));
        }
        for (const auto &child: children)
            if (child.key == childKey)
                return &child;
        return nullptr;
    }

    // Type checking helper methods
    bool isRoot() const { return type == NodeType::ROOT; }
    bool isSection() const { return type == NodeType::SECTION; }
    bool isValue() const { return type == NodeType::VALUE; }
    bool isContainer() const { return type == NodeType::ROOT || type == NodeType::SECTION; }
};

// --------------------------------------------------------------------------------
// Type conversion utilities

// Convert strings to basic types
// For numeric-like types, enforce strict parsing: only allow optional surrounding whitespace
// and reject any trailing non-whitespace characters (e.g., "42abc" is invalid)
template<typename T> T fromString(const std::string &str)
{
    std::istringstream iss(str);
    if constexpr (std::is_same_v<T, std::string>) {
        return str;
    } else {
        T value;
        iss >> value;
        if (iss.fail())
            throw std::runtime_error("Bad conversion from string: " + str);

        // Consume trailing whitespace and ensure we reached the end
        iss >> std::ws;
        if (!iss.eof())
            throw std::runtime_error("Bad conversion (trailing characters) from string: " + str);

        return value;
    }
}

// Specialized bool conversion for user-friendly values
template<> inline bool fromString<bool>(const std::string &str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on")
        return true;
    else if (lower == "false" || lower == "0" || lower == "no" || lower == "off")
        return false;
    throw std::runtime_error(fmt::format("Invalid boolean value: {} (expected: true/false, 1/0, yes/no, on/off)", str));
}

// --------------------------------------------------------------------------------
// Type traits

// Trait to detect std::vector<...>
template<typename T> struct is_vector : std::false_type { };

template<typename U, typename Alloc> struct is_vector<std::vector<U, Alloc>> : std::true_type { };

// Trait to detect std::variant<...>
template<typename T> struct is_variant : std::false_type { };

template<typename... Types> struct is_variant<std::variant<Types...>> : std::true_type { };

} // namespace cfg2