#pragma once
#include <string>
#include <vector>

namespace gwmilter {

struct header_item {
    header_item(std::string n, std::string v, unsigned int i, bool m)
        : name{std::move(n)}, value{std::move(v)}, index{i}, modified{m}
    { }

    std::string name;
    std::string value;
    unsigned int index = 0;
    bool modified = false;
};

// std::vector, because order is important
using headers_type = std::vector<header_item>;

} // end namespace gwmilter
