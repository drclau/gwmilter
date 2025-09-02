#include "config.hpp"
#include <stdexcept>
#include <unordered_set>

namespace cfg2 {

// Implementation of custom deserializer for Config
template<> Config deserialize<Config>(const ConfigNode &node)
{
    Config config{};
    std::unordered_set<std::string> foundSections;

    for (const auto &child: node.children) {
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
            }
        } else {
            // This is an unknown static section (no encryption_protocol field)
            throw std::invalid_argument("Unknown static section '[" + child.key + "]' in configuration");
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
