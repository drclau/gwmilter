#pragma once
#include <random>
#include <string>

namespace gwmilter {

class uid_generator {
public:
    std::string generate();

private:
    static std::mt19937_64 initialize_rng()
    {
        std::random_device rd;
        return std::mt19937_64(rd());
    }

    static thread_local std::mt19937_64 rng;
};

} // namespace gwmilter
