#pragma once
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace gwmilter::utils::string {

std::string str_err(int errnum);

std::string set_to_string(const std::set<std::string> &src);

std::string to_lower(std::string_view src);

bool iequals(std::string_view a, std::string_view b);

} // namespace gwmilter::utils::string
