#include "config_manager.hpp"
#include "core.hpp"
#include "signal_handler.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
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

int main()
{
    // Show current working directory
    std::cout << "Running in directory: " << std::filesystem::current_path() << "\n\n";

    // Create ConfigManager instance
    // const std::string configFile = "../src/cfg2/testdata/demo_config.ini";
    const std::string configFile = "src/cfg2/testdata/demo_config.ini";
    ConfigManager configMgr;

    try {
        configMgr.initialize(configFile);
        std::cout << "Successfully initialized ConfigManager with " << configFile << "\n";
    } catch (const std::exception &e) {
        std::cerr << "Failed to initialize ConfigManager: " << e.what() << "\n";
        return 1;
    }

    // Initialize signal handler
    if (!SignalHandler::initialize()) {
        std::cerr << "Failed to initialize signal handler\n";
        return 1;
    }

    // Get initial configuration
    auto config = configMgr.getConfig();
    if (!config) {
        std::cerr << "No configuration available\n";
        return 1;
    }

    printConfig(*config);

    std::cout << "=== Demo Loop ===\n";
    std::cout << "This demo will run in a loop checking for config changes.\n";
    std::cout << "To test SIGHUP reload:\n";
    std::cout << "1. Edit the config file: " << configFile << "\n";
    std::cout << "2. Send SIGHUP signal: kill -HUP " << getpid() << "\n";
    std::cout << "3. Watch the configuration reload automatically\n";
    std::cout << "Press Ctrl+C to exit.\n\n";

    // Test cases for the find_match functionality
    std::vector<std::string> testValues = {"pgp-user@example.com", "user-smime@example.com", "user-pdf@example.com",
                                           "user@example.com"};

    // Main loop - check for signals and demonstrate current config
    int iteration = 0;
    while (true) {
        // Check if reload was requested via SIGHUP
        if (SignalHandler::checkAndClearReloadRequest()) {
            std::cout << "\n*** SIGHUP received - reloading configuration ***\n";
            if (configMgr.reload()) {
                // Get the updated config
                config = configMgr.getConfig();
                if (config)
                    printConfig(*config);
            }
        }

        // Every 10 iterations, show current match testing
        if (iteration % 10 == 0) {
            std::cout << "Match Testing (iteration " << iteration << "):\n";
            for (const auto &testValue: testValues) {
                auto *matchedSection = config->find_match(testValue);
                if (matchedSection) {
                    std::cout << "  '" << testValue << "' -> [" << matchedSection->sectionName << "] ("
                              << matchedSection->encryption_protocol << ")\n";
                } else {
                    std::cout << "  '" << testValue << "' -> No match\n";
                }
            }
            std::cout << "\n";
        }

        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        iteration++;
    }

    return 0;
}