#include "signal_handler.hpp"
#include <csignal>
#include <iostream>
#include <unistd.h>

namespace cfg2 {

std::atomic<bool> SignalHandler::reload_requested_{false};

void SignalHandler::sighupHandler(int signal)
{
    // Signal handlers should only do async-signal-safe operations
    // Setting an atomic bool is safe
    if (signal == SIGHUP)
        reload_requested_.store(true);
}

bool SignalHandler::initialize()
{
    // Install SIGHUP handler
    struct sigaction sa;
    sa.sa_handler = sighupHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, nullptr) != 0) {
        std::cerr << "SignalHandler: Failed to install SIGHUP handler" << std::endl;
        return false;
    }

    std::cout << "SignalHandler: SIGHUP handler installed successfully" << std::endl;
    std::cout << "SignalHandler: Send SIGHUP to this process (PID: " << getpid() << ") to trigger config reload"
              << std::endl;
    return true;
}

bool SignalHandler::checkAndClearReloadRequest()
{
    return reload_requested_.exchange(false);
}

bool SignalHandler::isReloadRequested()
{
    return reload_requested_.load();
}

} // namespace cfg2