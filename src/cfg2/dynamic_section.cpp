#include "dynamic_section.hpp"
#include <stdexcept>

namespace cfg2 {

// Static member definition
std::unordered_map<std::string, SectionFactory> DynamicSectionRegistry::factories;

void DynamicSectionRegistry::registerFactory(const std::string &type, const SectionFactory &factory)
{
    factories[type] = factory;
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