#pragma once

#include "config_node.hpp"
#include "deserializer.hpp"
#include <functional>
#include <memory>
#include <regex>
#include <vector>

namespace cfg2 {

// --------------------------------------------------------------------------------
// Section support - unified base for both static and dynamic sections

// Base class for all sections
struct BaseSection {
    std::string sectionName;
    virtual ~BaseSection() = default;
};

// Base class for all dynamic sections (extends BaseSection with matching functionality)
struct BaseDynamicSection : BaseSection {
    std::string type;
    std::vector<std::string> match; // Raw regex patterns from INI
    std::vector<std::regex> compiledMatches; // Compiled regex patterns

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
        compiledMatches.reserve(match.size());
        for (const auto &pattern: match) {
            try {
                compiledMatches.emplace_back(pattern);
            } catch (const std::regex_error &e) {
                throw std::invalid_argument("Invalid regex pattern '" + pattern + "': " + e.what());
            }
        }
    }
};

// Factory function type for creating static sections
using StaticSectionFactory = std::function<std::unique_ptr<BaseSection>(const ConfigNode &)>;

// Factory function type for creating dynamic sections
using DynamicSectionFactory = std::function<std::unique_ptr<BaseDynamicSection>(const ConfigNode &)>;

// Registry for static sections only
class StaticSectionRegistry {
public:
    static void registerFactory(const std::string &sectionName, const StaticSectionFactory &factory);
    static std::unique_ptr<BaseSection> create(const std::string &sectionName, const ConfigNode &node);
    static bool hasSection(const std::string &sectionName);
};

// Registry for dynamic sections only
class DynamicSectionRegistry {
public:
    static void registerFactory(const std::string &type, const DynamicSectionFactory &factory);
    static std::unique_ptr<BaseDynamicSection> create(const std::string &type, const ConfigNode &node);
    static bool hasType(const std::string &type);
};

// Macro to register static section types
#define REGISTER_STATIC_SECTION(Type, SectionName, ...)                                                                \
    static_assert(std::is_base_of_v<BaseSection, Type>, #Type " must inherit from BaseSection");                       \
    REGISTER_STRUCT(Type, __VA_ARGS__)                                                                                 \
    namespace {                                                                                                        \
    struct Type##StaticRegistrar {                                                                                     \
        Type##StaticRegistrar()                                                                                        \
        {                                                                                                              \
            StaticSectionRegistry::registerFactory(SectionName,                                                        \
                                                   [](const ConfigNode &node) -> std::unique_ptr<BaseSection> {        \
                                                       auto obj = std::make_unique<Type>(deserialize<Type>(node));     \
                                                       obj->sectionName = node.key;                                    \
                                                       return obj;                                                     \
                                                   });                                                                 \
        }                                                                                                              \
    };                                                                                                                 \
    static Type##StaticRegistrar Type##_static_registrar_instance;                                                     \
    }

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
                    auto obj = std::make_unique<Type>(deserialize<Type>(node));                                        \
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