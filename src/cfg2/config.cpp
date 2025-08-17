#include "config.hpp"
#include <stdexcept>

namespace cfg2 {

// Implementation of custom deserializer for Config
template<> Config deserialize<Config>(const ConfigNode &node)
{
    Config config{};

    // Process all sections in order using separated registries
    for (const auto &child: node.children) {
        // Handle static sections by name
        if (StaticSectionRegistry::hasSection(child.key)) {
            auto section = StaticSectionRegistry::create(child.key, child);

            if (auto *generalSection = dynamic_cast<GeneralSection *>(section.get()))
                config.general = *generalSection;
        }
        // Handle dynamic sections by their encryption_protocol
        else
        {
            const ConfigNode *protocolNode = child.findChild("encryption_protocol");
            if (protocolNode && DynamicSectionRegistry::hasType(protocolNode->value)) {
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
        }
    }

    // Perform cross-section validation
    config.validate();

    return config;
}


} // namespace cfg2
