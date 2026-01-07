#pragma once
#include <string>

namespace gwmilter {

// Simple wrapper for milter
class milter {
public:
    explicit milter(const std::string &socket, unsigned long flags = -1, int timeout = -1, int backlog = -1,
                    int debug_level = -1);

    milter(const milter &) = delete;
    milter &operator=(const milter &) = delete;

    void run();

private:
    std::string socket_;
};

} // namespace gwmilter
