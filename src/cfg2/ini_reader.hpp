#pragma once

#include "config_node.hpp"
#include <string>

namespace cfg2 {

ConfigNode parseIniFile(const std::string &filename);

} // namespace cfg2