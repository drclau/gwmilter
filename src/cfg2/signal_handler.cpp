#include "signal_handler.hpp"
#include <csignal>
#include <unistd.h>

namespace cfg2 {

volatile sig_atomic_t SignalHandler::reload_requested_{0};
std::once_flag SignalHandler::init_flag_{};
bool SignalHandler::init_ok_ = false;

void SignalHandler::sighupHandler(int signal)
{
    // Signal handlers should only do async-signal-safe operations.
    // Writing to a volatile sig_atomic_t is async-signal-safe.
    if (signal == SIGHUP)
        reload_requested_ = 1;
}

bool SignalHandler::initialize()
{
    std::call_once(init_flag_, [] {
        // Install SIGHUP handler
        struct sigaction sa{}; // zero-initialize
        sa.sa_handler = sighupHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART; // restart interrupted syscalls where possible

        if (sigaction(SIGHUP, &sa, nullptr) != 0) {
            init_ok_ = false;
            return;
        }
        init_ok_ = true;
    });

    return init_ok_;
}

bool SignalHandler::checkAndClearReloadRequest()
{
    // Note: We deliberately accept that multiple SIGHUPs may coalesce into
    // a single reload event. This keeps the handler simple and signal-safe.
    if (reload_requested_) {
        reload_requested_ = 0;
        return true;
    }
    return false;
}

bool SignalHandler::isReloadRequested()
{
    return reload_requested_ != 0;
}

} // namespace cfg2
