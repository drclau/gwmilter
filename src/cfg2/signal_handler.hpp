#pragma once

#include <atomic>

namespace cfg2 {

class SignalHandler {
private:
    static std::atomic<bool> reload_requested_;
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