#include "ini_reader.hpp"
#include <SimpleIni.h>
#include <stdexcept>

namespace cfg2 {

ConfigNode parseIniFile(const std::string &filename)
{
    CSimpleIniA ini;

    // Load the INI file
    SI_Error rc = ini.LoadFile(filename.c_str());
    if (rc != SI_OK) {
        throw std::runtime_error("Failed to parse INI file '" + filename + "' (error code: " + std::to_string(rc) +
                                 ")");
    }

    // Create root config node
    ConfigNode root{"config", "", {}, NodeType::ROOT};

    // Get all sections
    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);
    // sort sections by the loading order. this is required for `find_match()` to work as expected.
    sections.sort([](const auto &a, const auto &b) { return a.nOrder < b.nOrder; });

    for (const auto &section: sections) {
        // Create a section node
        ConfigNode sectionNode{section.pItem, "", {}, NodeType::SECTION};

        // Get all keys for this section
        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys(section.pItem, keys);
        // sort keys by the loading order. this is not strictly necessary, but it is consistent with sections sorting.
        keys.sort([](const auto &a, const auto &b) { return a.nOrder < b.nOrder; });

        for (const auto &key: keys) {
            // Get the value for this key
            const char *value = ini.GetValue(section.pItem, key.pItem, "");

            // Create key-value node and add to section
            sectionNode.children.emplace_back(ConfigNode{key.pItem, value ? value : "", {}, NodeType::VALUE});
        }

        // Add section to root
        root.children.push_back(std::move(sectionNode));
    }

    return root;
}

} // namespace cfg2