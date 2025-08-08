#include "section_registry.hpp"
#include <stdexcept>

namespace cfg2 {

// Static member definitions
std::unordered_map<std::string, SectionFactory> SectionRegistry::factories;

// Unified SectionRegistry implementation
void SectionRegistry::registerFactory(const std::string &sectionName, const SectionFactory &factory)
{
    factories[sectionName] = factory;
}

std::unique_ptr<BaseSection> SectionRegistry::create(const std::string &sectionName, const ConfigNode &node)
{
    auto it = factories.find(sectionName);
    if (it != factories.end())
        return it->second(node);
    throw std::runtime_error("Unknown section: " + sectionName);
}

bool SectionRegistry::hasSection(const std::string &sectionName)
{
    return factories.find(sectionName) != factories.end();
}

// DynamicSectionRegistry implementation for backward compatibility
void DynamicSectionRegistry::registerFactory(
    const std::string &type, const std::function<std::unique_ptr<BaseDynamicSection>(const ConfigNode &)> &factory)
{
    // Register dynamic sections by their protocol type in the unified registry
    // Dynamic sections are looked up by their encryption_protocol value
    SectionRegistry::registerFactory(type, [factory](const ConfigNode &node) -> std::unique_ptr<BaseSection> {
        return std::unique_ptr<BaseSection>(factory(node).release());
    });
}

std::unique_ptr<BaseDynamicSection> DynamicSectionRegistry::create(const std::string &type, const ConfigNode &node)
{
    auto section = SectionRegistry::create(type, node);
    auto dynamicSection = dynamic_cast<BaseDynamicSection *>(section.release());
    if (!dynamicSection)
        throw std::runtime_error("Section type '" + type + "' is not a dynamic section");
    return std::unique_ptr<BaseDynamicSection>(dynamicSection);
}

bool DynamicSectionRegistry::hasType(const std::string &type)
{
    return SectionRegistry::hasSection(type);
}

} // namespace cfg2