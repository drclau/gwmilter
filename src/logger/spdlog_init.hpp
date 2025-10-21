#pragma once

namespace cfg2 {
struct GeneralSection;
}

namespace gwmilter::logging {

// Initialize spdlog from cfg2 configuration
void init_spdlog(const cfg2::GeneralSection &general_section);

} // namespace gwmilter::logging
