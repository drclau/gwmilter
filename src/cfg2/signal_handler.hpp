#pragma once

#include <csignal>
#include <mutex>

namespace cfg2 {

class SignalHandler {
private:
    // Set only from the async signal handler; read/cleared from normal code.
    // Using volatile sig_atomic_t ensures async-signal-safe access from handlers.
    static volatile sig_atomic_t reload_requested_;
    static std::once_flag init_flag_;
    static bool init_ok_;
    static void sighupHandler(int signal);

public:
    // Initialize signal handler
    static bool initialize();

    // Check if reload was requested and reset the flag
    static bool checkAndClearReloadRequest();

    // Check if reload was requested without clearing the flag
    static bool isReloadRequested();
};

} // namespace cfg2
