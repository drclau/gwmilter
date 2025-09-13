#pragma once

#include <atomic>
#include <csignal>
#include <thread>

namespace gwmilter {

// Installs a dedicated sigwait() thread to handle POSIX signals.
// - SIGHUP: log only for now
// - SIGTERM/SIGINT: call libmilter's smfi_stop() and exit the thread
class SignalManager {
public:
    SignalManager();
    ~SignalManager();

private:
    void signalLoop(sigset_t set);

    std::atomic<bool> running_{true};
    std::thread signal_thread_;
    sigset_t old_set_{}; // previous thread signal mask
};

} // namespace gwmilter
