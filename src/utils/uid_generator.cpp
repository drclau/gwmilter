#include "uid_generator.hpp"
#include <fmt/core.h>

namespace gwmilter {

thread_local std::mt19937_64 uid_generator::rng = uid_generator::initialize_rng();


std::string uid_generator::generate()
{
    std::uniform_int_distribution<std::uint32_t> uni(0, std::numeric_limits<std::uint32_t>::max());
    return fmt::format("{:08X}", uni(rng));
}

} // namespace gwmilter
