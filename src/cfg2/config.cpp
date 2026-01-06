#include "config.hpp"
#include "utils/string.hpp"
#include <fmt/core.h>
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
            throw std::invalid_argument(
                fmt::format("Global keys are not allowed in configuration; found key: '{}'", child.key));

        if (StaticSectionRegistry::hasSection(child.key)) {
            // This is a static section
            if (foundSections.find(child.key) != foundSections.end())
                throw std::invalid_argument(fmt::format("Duplicate static section '[{}]' encountered", child.key));

            foundSections.insert(child.key);
            auto section = StaticSectionRegistry::create(child.key, child);

            if (auto *generalSection = dynamic_cast<GeneralSection *>(section.get()))
                config.general = *generalSection;
        } else if (const ConfigNode *protocolNode = child.findChild("encryption_protocol")) {
            // This is a dynamic section
            const std::string protocol = gwmilter::utils::string::to_lower(protocolNode->value);
            if (DynamicSectionRegistry::hasType(protocol)) {
                auto section = DynamicSectionRegistry::create(protocol, child);
                if (dynamic_cast<BaseEncryptionSection *>(section.get())) {
                    // Cast succeeded, safe to transfer ownership
                    config.encryptionSections.emplace_back(std::unique_ptr<BaseEncryptionSection>(
                        static_cast<BaseEncryptionSection *>(section.release())));
                } else {
                    throw std::runtime_error("Dynamic section type '" + protocolNode->value +
                                             "' does not derive from BaseEncryptionSection");
                }
            } else {
                throw std::invalid_argument(
                    fmt::format("Unknown dynamic section type '{}' in section '[{}]'", protocolNode->value, child.key));
            }
        } else {
            throw std::invalid_argument(fmt::format("Unknown static section '[{}]'", child.key));
        }
    }

    // Validate that all mandatory static sections are present
    auto mandatorySections = StaticSectionRegistry::getMandatorySections();
    for (const auto &mandatorySection: mandatorySections)
        if (foundSections.find(mandatorySection) == foundSections.end())
            throw std::invalid_argument(
                fmt::format("Required section '[{}]' is missing from configuration", mandatorySection));

    // Perform cross-section validation
    config.validate();

    return config;
}


} // namespace cfg2
