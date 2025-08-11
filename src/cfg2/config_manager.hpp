#pragma once

#include "config.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace cfg2 {

class ConfigManager {
private:
    std::shared_ptr<Config> current_config_;
    std::mutex config_mutex_;
    std::string config_file_path_;

public:
    // Constructor - can optionally initialize with config file
    ConfigManager() = default;
    explicit ConfigManager(const std::string &config_file);

    // Initialize with config file path (if not done in constructor)
    void initialize(const std::string &config_file);

    // Get current configuration (thread-safe)
    std::shared_ptr<const Config> getConfig() const;

    // Reload configuration from file
    bool reload();

    // Check if manager is initialized
    bool isInitialized() const;

    ~ConfigManager();

    // Non-copyable but movable
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;
    ConfigManager(ConfigManager &&) = delete;
    ConfigManager &operator=(ConfigManager &&) = delete;
};

} // namespace cfg2