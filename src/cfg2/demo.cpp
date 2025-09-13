#include "config_manager.hpp"
#include "core.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <pthread.h>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>

using namespace cfg2;

void printConfig(const Config &config)
{
    std::cout << "\n=== Configuration Status ===\n";
    std::cout << "Milter Socket: " << config.general.milter_socket << "\n";
    std::cout << "SMTP Server: " << config.general.smtp_server << "\n";
    std::cout << "Log Type: " << config.general.log_type << "\n";
    std::cout << "Encryption Sections: " << config.encryptionSections.size() << "\n";
    for (const auto &section: config.encryptionSections)
        std::cout << "  - [" << section->sectionName << "] protocol: " << section->encryption_protocol << "\n";
    std::cout << "============================\n\n";
}

// Modern C++17 signal handling using sigwait()
class DemoSignalManager {
public:
    enum class SignalEvent {
        None,
        Reload,    // SIGHUP
        Shutdown   // SIGTERM/SIGINT
    };

private:
    std::atomic<bool> running_{true};
    std::thread signal_thread_;
    std::mutex event_mutex_;
    std::condition_variable event_cv_;
    SignalEvent pending_event_{SignalEvent::None};
    sigset_t old_set_{}; // previous signal mask to restore

public:
    explicit DemoSignalManager() {
        // Block signals for all threads - must be done before creating any threads
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGHUP);
        sigaddset(&set, SIGTERM);
        sigaddset(&set, SIGINT);

        // Save previous mask so we can restore it later
        if (pthread_sigmask(SIG_BLOCK, &set, &old_set_) != 0) {
            throw std::runtime_error("Failed to block signals");
        }

        // Start signal handling thread
        signal_thread_ = std::thread([this, set]() {
            signalLoop(set);
        });
    }

    ~DemoSignalManager() {
        // Ensure the waiting sigwait() unblocks so the thread can exit
        running_ = false;
        if (signal_thread_.joinable()) {
            // Option A: send SIGINT to wake sigwait and trigger a clean exit
            pthread_kill(signal_thread_.native_handle(), SIGINT);
            signal_thread_.join();
        }
        // Restore the original signal mask for this thread
        pthread_sigmask(SIG_SETMASK, &old_set_, nullptr);
    }

    // Wait for a signal event with timeout
    SignalEvent waitForEvent(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(event_mutex_);

        if (event_cv_.wait_for(lock, timeout, [this] { return pending_event_ != SignalEvent::None; })) {
            SignalEvent event = pending_event_;
            pending_event_ = SignalEvent::None;
            return event;
        }

        return SignalEvent::None;
    }

private:
    void signalLoop(sigset_t set) {
        int signal;
        while (true) {
            int rc = sigwait(&set, &signal);
            if (!running_)
                break;
            if (rc != 0) {
                std::cerr << "sigwait failed: " << rc << "\n";
                break;
            }
            switch (signal) {
            case SIGHUP:
                std::cout << "\n*** Received SIGHUP signal ***\n";
                {
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    pending_event_ = SignalEvent::Reload;
                }
                event_cv_.notify_one();
                break;

            case SIGTERM:
                std::cout << "\n*** Received SIGTERM signal ***\n";
                {
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    pending_event_ = SignalEvent::Shutdown;
                }
                event_cv_.notify_one();
                return;  // Exit signal loop - shutdown requested

            case SIGINT:
                std::cout << "\n*** Received SIGINT signal (Ctrl+C) ***\n";
                {
                    std::lock_guard<std::mutex> lock(event_mutex_);
                    pending_event_ = SignalEvent::Shutdown;
                }
                event_cv_.notify_one();
                return;  // Exit signal loop - shutdown requested
            }
        }
    }
};

int main()
{
    // Show current working directory
    std::cout << "Running in directory: " << std::filesystem::current_path() << "\n\n";

    // Create ConfigManager instance
    const std::string configFile = "src/cfg2/testdata/config.ini";

    try {
        ConfigManager configMgr(configFile);

        // Initialize modern signal handler
        DemoSignalManager signalMgr;

        // Get initial configuration
        auto config = configMgr.getConfig();
        if (!config) {
            std::cerr << "No configuration available\n";
            return 1;
        }

        printConfig(*config);

        std::cout << "=== Demo Loop ===\n";
        std::cout << "This demo will run event-driven, waiting for signals.\n";
        std::cout << "To test SIGHUP reload:\n";
        std::cout << "1. Edit the config file: " << configFile << "\n";
        std::cout << "2. Send SIGHUP signal: kill -HUP " << getpid() << "\n";
        std::cout << "3. Watch the configuration reload immediately\n";
        std::cout << "Press Ctrl+C or send SIGTERM to exit gracefully.\n\n";

        // Test cases for the find_match functionality
        std::vector<std::string> testValues = {"pgp-user@example.com", "user-smime@example.com", "user-pdf@example.com",
                                               "user@example.com"};

        // Show initial match testing
        std::cout << "Initial Match Testing:\n";
        for (const auto &testValue: testValues) {
            auto *matchedSection = config->find_match(testValue);
            if (matchedSection) {
                std::cout << "  '" << testValue << "' -> [" << matchedSection->sectionName << "] ("
                          << matchedSection->encryption_protocol << ")\n";
            } else {
                std::cout << "  '" << testValue << "' -> No match\n";
            }
        }
        std::cout << "\nWaiting for signals...\n\n";

        // Event-driven main loop
        while (true) {
            // Wait for signal events (with 5 second timeout for periodic status)
            auto event = signalMgr.waitForEvent(std::chrono::seconds(5));

            switch (event) {
            case DemoSignalManager::SignalEvent::Reload:
                std::cout << "Processing configuration reload...\n";
                if (configMgr.reload()) {
                    config = configMgr.getConfig();
                    if (config) {
                        printConfig(*config);

                        // Show updated match testing
                        std::cout << "Updated Match Testing:\n";
                        for (const auto &testValue: testValues) {
                            auto *matchedSection = config->find_match(testValue);
                            if (matchedSection) {
                                std::cout << "  '" << testValue << "' -> [" << matchedSection->sectionName << "] ("
                                          << matchedSection->encryption_protocol << ")\n";
                            } else {
                                std::cout << "  '" << testValue << "' -> No match\n";
                            }
                        }
                        std::cout << "\nWaiting for signals...\n\n";
                    }
                } else {
                    std::cerr << "Failed to reload configuration!\n";
                }
                break;

            case DemoSignalManager::SignalEvent::Shutdown:
                std::cout << "Shutting down gracefully...\n";
                return 0;

            case DemoSignalManager::SignalEvent::None:
                // Timeout - show periodic status
                std::cout << "[" << std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()
                    << "] Demo still running... (send SIGHUP to reload, Ctrl+C to exit)\n";
                break;
            }
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
