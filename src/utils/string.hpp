#pragma once
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace gwmilter::utils::string {

std::string str_err(int errnum);

std::string set_to_string(const std::set<std::string> &src);

} // namespace gwmilter::utils::string
