#include "config_manager.hpp"
#include "core.hpp"
#include "logger/logger.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <unistd.h>

using namespace cfg2;

// Forward declarations
void printConfig(const Config &config);
void runMatchTesting(const Config &config, const std::string &label);

// Signal handler based on SignalManager but simplified for demo
class DemoSignalHandler {
public:
    explicit DemoSignalHandler(ConfigManager &config_mgr);
    ~DemoSignalHandler();

private:
    void signalLoop(sigset_t set);

    std::atomic<bool> running_{true};
    std::thread signal_thread_;
    sigset_t old_set_{};
    ConfigManager &config_mgr_;
};

DemoSignalHandler::DemoSignalHandler(ConfigManager &config_mgr)
    : config_mgr_(config_mgr)
{
    // Block signals in the current thread so the dedicated thread can receive them via sigwait()
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &set, &old_set_) != 0)
        throw std::runtime_error("DemoSignalHandler: Failed to block signals");

    signal_thread_ = std::thread([this, set]() { signalLoop(set); });
    std::cout << "Signals installed: SIGHUP, SIGINT, SIGTERM\n";
}

DemoSignalHandler::~DemoSignalHandler()
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

void DemoSignalHandler::signalLoop(sigset_t set)
{
    int sig = 0;
    for (;;) {
        int rc = sigwait(&set, &sig);
        if (!running_)
            break; // ignore events during teardown
        if (rc != 0) {
            std::cerr << "DemoSignalHandler: sigwait failed: " << rc << "\n";
            break;
        }

        switch (sig) {
        case SIGHUP:
            std::cout << "\n*** Received SIGHUP signal (reload requested) ***\n";
            if (config_mgr_.reload()) {
                std::cout << "Configuration reloaded successfully\n";
                // Show updated configuration and match testing
                auto config = config_mgr_.getConfig();
                if (config) {
                    printConfig(*config);
                    runMatchTesting(*config, "Updated");
                }
            } else {
                std::cout << "Failed to reload configuration\n";
            }
            break;
        case SIGTERM:
            std::cout << "\n*** Received SIGTERM signal (shutdown requested) ***\n";
            std::exit(0); // Exit the demo
        case SIGINT:
            std::cout << "\n*** Received SIGINT signal (shutdown requested) ***\n";
            std::exit(0); // Exit the demo
        default:
            break;
        }
    }
}

void printConfig(const Config &config)
{
    std::cout << "\n=== Configuration Status ===\n";
    std::cout << "Milter Socket: " << config.general.milter_socket << "\n";
    std::cout << "SMTP Server: " << config.general.smtp_server << "\n";
    std::cout << "Log Type: " << config.general.log_type << "\n";
    std::cout << "Encryption Sections: " << config.encryptionSections.size() << "\n";
    for (const auto &section: config.encryptionSections)
        std::cout << "  - [" << section->sectionName << "] protocol: " << toString(section->encryption_protocol)
                  << "\n";
    std::cout << "============================\n\n";
}

void runMatchTesting(const Config &config, const std::string &label)
{
    std::vector<std::string> testValues = {"pgp-user@example.com", "user-smime@example.com", "user-pdf@example.com",
                                           "user@example.com"};

    std::cout << label << " Match Testing:\n";
    for (const auto &testValue: testValues) {
        auto *matchedSection = config.find_match(testValue);
        if (matchedSection) {
            std::cout << "  '" << testValue << "' -> [" << matchedSection->sectionName << "] ("
                      << toString(matchedSection->encryption_protocol) << ")\n";
        } else {
            std::cout << "  '" << testValue << "' -> No match\n";
        }
    }
    std::cout << "\n";
}


int main()
{
    // Initialize spdlog with all log levels enabled
    spdlog::set_level(spdlog::level::trace);

    std::cout << "Running in directory: " << std::filesystem::current_path() << "\n\n";

    try {
        ConfigManager configMgr("src/cfg2/testdata/config.ini");

        // Initialize signal handler
        DemoSignalHandler signalMgr(configMgr);

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
        std::cout << "1. Edit the config file: " << configMgr.path() << "\n";
        std::cout << "2. Send SIGHUP signal: kill -HUP " << getpid() << "\n";
        std::cout << "3. Watch the configuration reload immediately\n";
        std::cout << "Press Ctrl+C or send SIGTERM to exit gracefully.\n\n";

        // Test cases for the find_match functionality
        std::vector<std::string> testValues = {"pgp-user@example.com", "user-smime@example.com", "user-pdf@example.com",
                                               "user@example.com"};

        // Show initial match testing
        runMatchTesting(*config, "Initial");
        std::cout << "\nWaiting for signals...\n\n";

        // Simple main loop - signals are handled in background by DemoSignalHandler
        std::cout << "Demo running... DemoSignalHandler will handle SIGTERM/SIGINT for shutdown.\n";
        std::cout << "Send SIGHUP to reload configuration: kill -HUP " << getpid() << "\n";
        std::cout << "Press Ctrl+C or send SIGTERM to exit gracefully.\n\n";

        // Keep the demo running until a signal terminates the process
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "["
                      << std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count()
                      << "] Demo still running... (Press Ctrl+C to exit)\n";
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
