#pragma once

#include "config_node.hpp"
#include "deserializer.hpp"
#include <functional>
#include <memory>
#include <regex>
#include <unordered_map>
#include <vector>

namespace cfg2 {

// --------------------------------------------------------------------------------
// Dynamic section support

// Base class for all dynamic sections
struct BaseDynamicSection {
    std::string sectionName;
    std::string type;
    std::vector<std::string> match; // Raw regex patterns from INI
    std::vector<std::regex> compiledMatches; // Compiled regex patterns

    virtual ~BaseDynamicSection() = default;

    // Check if any of the compiled regex patterns match the given value
    bool matches(const std::string &value) const
    {
        for (const auto &regex: compiledMatches)
            if (std::regex_search(value, regex))
                return true;
        return false;
    }

    // Compile regex patterns from match strings
    void compileMatches()
    {
        compiledMatches.clear();
        for (const auto &pattern: match) {
            try {
                compiledMatches.emplace_back(pattern);
            } catch (const std::regex_error &e) {
                throw std::invalid_argument("Invalid regex pattern '" + pattern + "': " + e.what());
            }
        }
    }
};

// Factory function type for creating dynamic sections
using SectionFactory = std::function<std::unique_ptr<BaseDynamicSection>(const ConfigNode &)>;

// Registry for dynamic section factories
class DynamicSectionRegistry {
    static std::unordered_map<std::string, SectionFactory> factories;

public:
    static void registerFactory(const std::string &type, const SectionFactory &factory);
    static std::unique_ptr<BaseDynamicSection> create(const std::string &type, const ConfigNode &node);
    static bool hasType(const std::string &type);
};

// Macro to register dynamic section types
#define REGISTER_DYNAMIC_SECTION(Type, TypeName, ...)                                                                  \
    static_assert(std::is_base_of_v<BaseDynamicSection, Type>, #Type " must inherit from BaseDynamicSection");         \
    REGISTER_STRUCT(Type, __VA_ARGS__)                                                                                 \
    namespace {                                                                                                        \
    struct Type##Registrar {                                                                                           \
        Type##Registrar()                                                                                              \
        {                                                                                                              \
            DynamicSectionRegistry::registerFactory(                                                                   \
                TypeName, [](const ConfigNode &node) -> std::unique_ptr<BaseDynamicSection> {                          \
                    auto obj = std::make_unique<Type>();                                                               \
                    *obj = deserialize<Type>(node);                                                                    \
                    obj->sectionName = node.key;                                                                       \
                    obj->type = TypeName;                                                                              \
                    obj->compileMatches();                                                                             \
                    return obj;                                                                                        \
                });                                                                                                    \
        }                                                                                                              \
    };                                                                                                                 \
    static Type##Registrar Type##_registrar_instance;                                                                  \
    }

} // namespace cfg2