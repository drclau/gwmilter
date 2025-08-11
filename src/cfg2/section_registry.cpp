#include "section_registry.hpp"
#include <stdexcept>

namespace cfg2 {

// Static member definitions
std::unordered_map<std::string, StaticSectionFactory> StaticSectionRegistry::factories;
std::unordered_map<std::string, DynamicSectionFactory> DynamicSectionRegistry::factories;

// StaticSectionRegistry implementation
void StaticSectionRegistry::registerFactory(const std::string &sectionName, const StaticSectionFactory &factory)
{
    factories.insert_or_assign(sectionName, factory);
}

std::unique_ptr<BaseSection> StaticSectionRegistry::create(const std::string &sectionName, const ConfigNode &node)
{
    auto it = factories.find(sectionName);
    if (it != factories.end())
        return it->second(node);
    throw std::runtime_error("Unknown static section: " + sectionName);
}

bool StaticSectionRegistry::hasSection(const std::string &sectionName)
{
    return factories.find(sectionName) != factories.end();
}

// DynamicSectionRegistry implementation
void DynamicSectionRegistry::registerFactory(const std::string &type, const DynamicSectionFactory &factory)
{
    factories.insert_or_assign(type, factory);
}

std::unique_ptr<BaseDynamicSection> DynamicSectionRegistry::create(const std::string &type, const ConfigNode &node)
{
    auto it = factories.find(type);
    if (it != factories.end())
        return it->second(node);
    throw std::runtime_error("Unknown dynamic section type: " + type);
}

bool DynamicSectionRegistry::hasType(const std::string &type)
{
    return factories.find(type) != factories.end();
}

} // namespace cfg2