#pragma once

#include "cfg2/config_manager.hpp"
#include <atomic>
#include <csignal>
#include <thread>

namespace gwmilter {

// Installs a dedicated sigwait() thread to handle POSIX signals.
// - SIGHUP: reload configuration via cfg2::ConfigManager
// - SIGTERM/SIGINT: call libmilter's smfi_stop() and exit the thread
class SignalManager {
public:
    explicit SignalManager(cfg2::ConfigManager &config_mgr);
    ~SignalManager();

private:
    void signalLoop(sigset_t set);

    std::atomic<bool> running_{true};
    std::thread signal_thread_;
    sigset_t old_set_{}; // previous thread signal mask
    cfg2::ConfigManager &config_mgr_;
};

} // namespace gwmilter
