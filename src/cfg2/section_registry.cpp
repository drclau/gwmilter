#include "section_registry.hpp"
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cfg2 {

namespace {
using StaticFactories = std::unordered_map<std::string, StaticSectionFactory>;
using DynamicFactories = std::unordered_map<std::string, DynamicSectionFactory>;
using MandatorySections = std::unordered_set<std::string>;

StaticFactories &staticFactories()
{
    static StaticFactories factories; // function-local static; thread-safe initialization
    return factories;
}

DynamicFactories &dynamicFactories()
{
    static DynamicFactories factories; // function-local static; thread-safe initialization
    return factories;
}

MandatorySections &mandatorySections()
{
    static MandatorySections sections; // function-local static; thread-safe initialization
    return sections;
}
} // namespace

// StaticSectionRegistry implementation
void StaticSectionRegistry::registerFactory(const std::string &sectionName, const StaticSectionFactory &factory,
                                            bool mandatory)
{
    staticFactories().insert_or_assign(sectionName, factory);
    if (mandatory)
        mandatorySections().insert(sectionName);
}

std::unique_ptr<BaseSection> StaticSectionRegistry::create(const std::string &sectionName, const ConfigNode &node)
{
    auto &f = staticFactories();
    auto it = f.find(sectionName);
    if (it != f.end())
        return it->second(node);
    throw std::runtime_error("Unknown static section: " + sectionName);
}

bool StaticSectionRegistry::hasSection(const std::string &sectionName)
{
    auto &f = staticFactories();
    return f.find(sectionName) != f.end();
}

bool StaticSectionRegistry::isMandatory(const std::string &sectionName)
{
    auto &m = mandatorySections();
    return m.find(sectionName) != m.end();
}

std::vector<std::string> StaticSectionRegistry::getMandatorySections()
{
    auto &m = mandatorySections();
    return std::vector<std::string>(m.begin(), m.end());
}

// DynamicSectionRegistry implementation
void DynamicSectionRegistry::registerFactory(const std::string &type, const DynamicSectionFactory &factory)
{
    dynamicFactories().insert_or_assign(type, factory);
}

std::unique_ptr<BaseDynamicSection> DynamicSectionRegistry::create(const std::string &type, const ConfigNode &node)
{
    auto &f = dynamicFactories();
    auto it = f.find(type);
    if (it != f.end())
        return it->second(node);
    throw std::runtime_error("Unknown dynamic section type: " + type);
}

bool DynamicSectionRegistry::hasType(const std::string &type)
{
    auto &f = dynamicFactories();
    return f.find(type) != f.end();
}

} // namespace cfg2