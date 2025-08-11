#include "config_manager.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

using namespace cfg2;

// Test to demonstrate memory safety during config reloads
TEST(MemorySafetyTest, ConfigReloadThreadSafety)
{
    std::cout << "=== Memory Safety Test ===\n";

    ConfigManager configMgr("src/cfg2/testdata/demo_config.ini");

    // Get initial config
    auto initial_config = configMgr.getConfig();
    std::cout << "Initial config loaded. SMTP server: " << initial_config->general.smtp_server << "\n";

    std::vector<std::thread> worker_threads;
    std::atomic<bool> stop_test{false};

    // Start 5 worker threads that hold onto config for extended periods
    for (int i = 0; i < 5; ++i) {
        worker_threads.emplace_back([&configMgr, &stop_test, i]() {
            while (!stop_test.load()) {
                // Get config and hold onto it
                auto config = configMgr.getConfig();
                if (!config)
                    continue;

                std::cout << "Thread " << i << " using config with SMTP: " << config->general.smtp_server << std::endl;

                // Simulate work with the config - this would be unsafe in the old implementation
                // if another thread called reload() during this sleep
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Continue accessing config after potential reload
                std::cout << "Thread " << i << " finished work with SMTP: " << config->general.smtp_server << std::endl;

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }

    // Main thread performs config reloads
    for (int reload_count = 0; reload_count < 3; ++reload_count) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "\n*** Performing reload #" << (reload_count + 1) << " ***\n";
        bool success = configMgr.reload();
        std::cout << "Reload " << (reload_count + 1) << " " << (success ? "successful" : "failed") << "\n";

        // Verify new config is accessible
        auto new_config = configMgr.getConfig();
        if (new_config)
            std::cout << "New config available with SMTP: " << new_config->general.smtp_server << "\n";
    }

    // Let worker threads finish their current work
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Stop all threads
    stop_test.store(true);
    for (auto &t: worker_threads)
        t.join();

    std::cout << "\n=== Memory Safety Test Completed Successfully ===\n";
    std::cout << "If you see this message, no memory corruption occurred!\n";

    SUCCEED(); // Test passed if we reach here without crashes
}