#include "string.hpp"
#include <set>
#include <string>
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

} // namespace gwmilter::utils::string
