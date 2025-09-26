#pragma once

#include "config_node.hpp"
#include <filesystem>

namespace cfg2 {

[[nodiscard]] ConfigNode parseIniFile(const std::filesystem::path &filename);

} // namespace cfg2
