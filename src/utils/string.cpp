#include "string.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <string_view>
#include <system_error>

namespace gwmilter::utils::string {

std::string str_err(int errnum)
{
    return std::system_category().message(errnum);
}


std::string set_to_string(const std::set<std::string> &src)
{
    std::string result;
    for (auto it = src.begin(); it != src.end();) {
        result += *it;
        if (++it != src.end())
            result += ", ";
    }
    return result;
}

std::string to_lower(std::string_view src)
{
    std::string lower(src);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

} // namespace gwmilter::utils::string
