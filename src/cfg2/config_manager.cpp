#include "config_manager.hpp"
#include "ini_reader.hpp"
#include "logger/logger.hpp"
#include <atomic>
#include <filesystem>

namespace cfg2 {

ConfigManager::ConfigManager(const std::filesystem::path &config_file)
    : config_file_path_(config_file)
{
    try {
        // Load and parse the configuration
        ConfigNode root = parseIniFile(config_file_path_);
        current_config_ = std::make_shared<const Config>(deserialize<Config>(root));
    } catch (const std::exception &e) {
        spdlog::error("ConfigManager: Failed to initialize with config file '{}': {}", config_file.string(), e.what());
        throw;
    }
}

std::shared_ptr<const Config> ConfigManager::getConfig() const
{
    return std::atomic_load_explicit(&current_config_, std::memory_order_acquire);
}

std::string ConfigManager::path() const
{
    return config_file_path_.string();
}

bool ConfigManager::reload()
{
    // Check if file exists
    if (!std::filesystem::exists(config_file_path_)) {
        spdlog::error("ConfigManager: Config file not found: {}", config_file_path_.string());
        return false;
    }

    try {
        spdlog::info("ConfigManager: Reloading configuration from {}", config_file_path_.string());

        // Load and parse the new configuration
        ConfigNode root = parseIniFile(config_file_path_);
        auto new_config_ptr = std::make_shared<const Config>(deserialize<Config>(root));

        // Replace the current configuration
        // The old Config object will be automatically destroyed when the last
        // shared_ptr referencing it goes out of scope (safe reference counting)
        std::atomic_store_explicit(&current_config_, new_config_ptr, std::memory_order_release);

        spdlog::info("ConfigManager: Configuration successfully reloaded");
        return true;

    } catch (const std::exception &e) {
        spdlog::error("ConfigManager: Failed to reload configuration: {}", e.what());
        spdlog::error("ConfigManager: Keeping current configuration");
        return false;
    }
}


} // namespace cfg2
