#pragma once

#include "config.hpp"
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace cfg2 {

class ConfigManager {
    std::shared_ptr<Config> current_config_;
    mutable std::mutex config_mutex_;
    std::filesystem::path config_file_path_;

public:
    explicit ConfigManager(const std::filesystem::path &config_file);

    // Get current configuration (thread-safe)
    std::shared_ptr<const Config> getConfig() const;

    // Expose config file path (as string)
    std::string path() const;

    // Reload configuration from file
    bool reload();

    ~ConfigManager() = default;

    // Non-copyable and non-movable
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;
    ConfigManager(ConfigManager &&) = delete;
    ConfigManager &operator=(ConfigManager &&) = delete;
};

} // namespace cfg2
