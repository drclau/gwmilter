#include "config.hpp"
#include "logger/logger.hpp"
#include <stdexcept>
#include <unordered_set>

namespace cfg2 {

// Implementation of custom deserializer for Config
template<> Config deserialize<Config>(const ConfigNode &node)
{
    if (!node.isRoot())
        throw std::invalid_argument("Config deserializer requires a root ConfigNode");

    Config config{};
    std::unordered_set<std::string> foundSections;

    for (const auto &child: node.children) {
        if (!child.isSection())
            throw std::invalid_argument("Global keys are not allowed in configuration; found key: '" + child.key + "'");

        if (StaticSectionRegistry::hasSection(child.key)) {
            // This is a static section
            foundSections.insert(child.key);
            auto section = StaticSectionRegistry::create(child.key, child);

            if (auto *generalSection = dynamic_cast<GeneralSection *>(section.get()))
                config.general = *generalSection;
        } else if (const ConfigNode *protocolNode = child.findChild("encryption_protocol")) {
            // This is a dynamic section
            if (DynamicSectionRegistry::hasType(protocolNode->value)) {
                auto section = DynamicSectionRegistry::create(protocolNode->value, child);
                if (dynamic_cast<BaseEncryptionSection *>(section.get())) {
                    // Cast succeeded, safe to transfer ownership
                    config.encryptionSections.emplace_back(std::unique_ptr<BaseEncryptionSection>(
                        static_cast<BaseEncryptionSection *>(section.release())));
                } else {
                    throw std::runtime_error("Dynamic section type '" + protocolNode->value +
                                             "' does not derive from BaseEncryptionSection");
                }
            } else {
                // Log warning, but allow for forward-compatability
                spdlog::warn(
                    "Unknown configuration dynamic section ignored: section = '[{}]', encryption_protocol = '{}'",
                    child.key, protocolNode->value);
            }
        } else {
            // Log warning, but allow for forward-compatability
            spdlog::warn("Unknown configuration static section '[{}]' ignored", child.key);
        }
    }

    // Validate that all mandatory static sections are present
    auto mandatorySections = StaticSectionRegistry::getMandatorySections();
    for (const auto &mandatorySection: mandatorySections)
        if (foundSections.find(mandatorySection) == foundSections.end())
            throw std::invalid_argument("Required section '[" + mandatorySection + "]' is missing from configuration");

    // Perform cross-section validation
    config.validate();

    return config;
}


} // namespace cfg2
