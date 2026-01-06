#include "dump_email.hpp"
#include "logger/logger.hpp"
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <string>

namespace gwmilter::utils {

dump_email::dump_email(const char *path, const char *prefix, const std::string &conn_id, const std::string &msg_id,
                       const std::string &headers, const std::string &body, bool eraseOnDestruct,
                       bool dump_email_on_panic)
    : erase_{eraseOnDestruct}
{
    if (!dump_email_on_panic) {
        // do nothing when disabled
        erase_ = false;
        return;
    }

    std::filesystem::path file_path(path);
    file_path /= fmt::format("{}{}-{}.eml", prefix, conn_id, msg_id);
    std::filesystem::create_directories(file_path.parent_path());

    std::ofstream ofs(file_path);
    ofs << headers << "\r\n" << body;

    file_ = file_path.string();
    spdlog::debug("Email dumped into {}", file_);
}


dump_email::~dump_email()
{
    if (erase_) {
        std::filesystem::remove(file_);
        spdlog::debug("Removed {}", file_);
    }
}

} // namespace gwmilter::utils
