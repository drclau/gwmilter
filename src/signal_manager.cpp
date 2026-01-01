#include "signal_manager.hpp"
#include "logger/logger.hpp"
#include "logger/spdlog_init.hpp"
#include <cassert>
#include <libmilter/mfapi.h>

namespace gwmilter {

SignalManager::SignalManager(cfg2::ConfigManager &config_mgr)
    : config_mgr_(config_mgr)
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
            spdlog::info("Received SIGHUP (reload requested)");
            if (config_mgr_.reload()) {
                // Reinitialize logging with new configuration
                auto new_config = config_mgr_.getConfig();
                assert(new_config != nullptr);
                try {
                    logging::init_spdlog(new_config->general);
                    spdlog::info("Configuration and logging reloaded successfully. NOTE: changes of milter settings "
                                 "require a full restart.");
                } catch (const std::exception &e) {
                    spdlog::error("Failed to reinitialize logging after config reload: {}", e.what());
                    spdlog::warn("Configuration reloaded but logging settings unchanged");
                }
            } else {
                spdlog::warn("Configuration reload failed; keeping current configuration");
            }
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
