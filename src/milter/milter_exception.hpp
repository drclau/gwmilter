#pragma once
#include <stdexcept>

namespace gwmilter {

class milter_exception : public std::runtime_error {
public:
    explicit milter_exception(const std::string &desc)
        : runtime_error{desc}
    { }
};

} // namespace gwmilter
