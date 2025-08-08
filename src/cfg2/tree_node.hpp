#pragma once

#include <string>
#include <vector>
#include <type_traits>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace cfg2 {

// Tree node structure for hierarchical data
struct TreeNode {
    std::string key;
    std::string value;
    std::vector<TreeNode> children;
    
    // Helper to find child by key
    const TreeNode* findChild(const std::string& childKey) const {
        for (const auto& child : children) {
            if (child.key == childKey) {
                return &child;
            }
        }
        return nullptr;
    }
};

// --------------------------------------------------------------------------------
// Type conversion utilities

// Convert strings to basic types
template<typename T>
T fromString(const std::string& str) {
    std::istringstream iss(str);
    if constexpr (std::is_same_v<T, std::string>) {
        return str;
    } else {
        T value;
        iss >> value;
        if (iss.fail()) {
            throw std::runtime_error("Bad conversion from string: " + str);
        }
        return value;
    }
}

// --------------------------------------------------------------------------------
// Type traits

// Trait to detect std::vector<...>
template<typename T>
struct is_vector : std::false_type {};

template<typename U, typename Alloc>
struct is_vector<std::vector<U,Alloc>> : std::true_type {};

// Trait to detect std::variant<...>
template<typename T>
struct is_variant : std::false_type {};

template<typename... Types>
struct is_variant<std::variant<Types...>> : std::true_type {};

} // namespace cfg2