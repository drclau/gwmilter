#pragma once
#include <string>

namespace gwmilter::utils {

class dump_email {
public:
    dump_email(const char *path, const char *prefix, const std::string &conn_id, const std::string &msg_id,
               const std::string &headers, const std::string &body, bool eraseOnDestruct);
    ~dump_email();

private:
    bool erase_;
    std::string file_;
};

} // namespace gwmilter::utils
