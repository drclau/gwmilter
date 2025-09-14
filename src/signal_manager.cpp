#include "signal_manager.hpp"
#include <libmilter/mfapi.h>
#include <spdlog/spdlog.h>

namespace gwmilter {

SignalManager::SignalManager()
{
    // Block signals in the current thread so the dedicated thread can receive them via sigwait()
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &set, &old_set_) != 0)
        throw std::runtime_error("SignalManager: Failed to block signals");

    signal_thread_ = std::thread([this, set]() { signalLoop(set); });
    spdlog::info("Signals installed: SIGHUP, SIGINT, SIGTERM");
}

SignalManager::~SignalManager()
{
    running_ = false;
    if (signal_thread_.joinable()) {
        // Wake sigwait() so the thread can exit; no-op when !running_
        pthread_kill(signal_thread_.native_handle(), SIGINT);
        signal_thread_.join();
    }
    // Restore previous signal mask for this thread
    pthread_sigmask(SIG_SETMASK, &old_set_, nullptr);
}

void SignalManager::signalLoop(sigset_t set)
{
    int sig = 0;
    for (;;) {
        int rc = sigwait(&set, &sig);
        if (!running_)
            break; // ignore events during teardown
        if (rc != 0) {
            spdlog::error("SignalManager: sigwait failed: {}", rc);
            break;
        }

        switch (sig) {
        case SIGHUP:
            spdlog::info("Received SIGHUP (reload requested) - not implemented");
            break;
        case SIGTERM:
            spdlog::info("Received SIGTERM (shutdown requested); stopping milter");
            smfi_stop();
            return; // exit thread
        case SIGINT:
            spdlog::info("Received SIGINT (shutdown requested); stopping milter");
            smfi_stop();
            return; // exit thread
        default:
            break;
        }
    }
}

} // namespace gwmilter
