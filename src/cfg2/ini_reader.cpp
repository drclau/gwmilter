#include "ini_reader.hpp"
#include <SimpleIni.h>
#include <fmt/core.h>
#include <stdexcept>

namespace cfg2 {

ConfigNode parseIniFile(const std::string &filename)
{
    CSimpleIniA ini;
    SI_Error rc = ini.LoadFile(filename.c_str());
    if (rc != SI_OK)
        throw std::runtime_error(fmt::format("Failed to parse INI file '{}' (error code: {})", filename, rc));

    ConfigNode root{"config", "", {}, NodeType::ROOT};

    // Add global keys directly to root (keys outside of any section)
    CSimpleIniA::TNamesDepend globalKeys;
    ini.GetAllKeys("", globalKeys);
    // sort global keys by loading order
    globalKeys.sort([](const auto &a, const auto &b) { return a.nOrder < b.nOrder; });

    for (const auto &key: globalKeys) {
        const char *value = ini.GetValue("", key.pItem, "");
        root.children.emplace_back(ConfigNode{key.pItem, value ? value : "", {}, NodeType::VALUE});
    }

    // Get all sections
    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);
    // sort sections by the loading order. this is required for `find_match()` to work as expected.
    sections.sort([](const auto &a, const auto &b) { return a.nOrder < b.nOrder; });

    for (const auto &section: sections) {
        ConfigNode sectionNode{section.pItem, "", {}, NodeType::SECTION};

        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys(section.pItem, keys);
        // sort keys by the loading order. this is not strictly necessary, but it is consistent with sections sorting.
        keys.sort([](const auto &a, const auto &b) { return a.nOrder < b.nOrder; });

        for (const auto &key: keys) {
            const char *value = ini.GetValue(section.pItem, key.pItem, "");
            sectionNode.children.emplace_back(ConfigNode{key.pItem, value ? value : "", {}, NodeType::VALUE});
        }

        root.children.push_back(std::move(sectionNode));
    }

    return root;
}

} // namespace cfg2
