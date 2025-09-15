#include "config_manager.hpp"
#include "ini_reader.hpp"
#include <filesystem>
#include <iostream>

namespace cfg2 {

ConfigManager::ConfigManager(const std::filesystem::path &config_file)
    : config_file_path_(config_file)
{
    try {
        // Load and parse the configuration
        ConfigNode root = parseIniFile(config_file_path_);
        current_config_ = std::make_shared<Config>(deserialize<Config>(root));

        std::cout << "ConfigManager: Successfully loaded configuration from " << config_file << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "ConfigManager: Failed to initialize with config file '" << config_file << "': " << e.what()
                  << std::endl;
        throw;
    }
}

std::shared_ptr<const Config> ConfigManager::getConfig() const
{
    std::lock_guard lock(config_mutex_);
    return current_config_;
}

std::string ConfigManager::path() const
{
    return config_file_path_;
}

bool ConfigManager::reload()
{
    std::lock_guard lock(config_mutex_);

    // Check if file exists
    if (!std::filesystem::exists(config_file_path_)) {
        std::cerr << "ConfigManager: Config file not found: " << config_file_path_ << std::endl;
        return false;
    }

    try {
        std::cout << "ConfigManager: Reloading configuration from " << config_file_path_ << std::endl;

        // Load and parse the new configuration
        ConfigNode root = parseIniFile(config_file_path_);
        auto new_config_ptr = std::make_shared<Config>(deserialize<Config>(root));

        // Replace the current configuration
        // The old Config object will be automatically destroyed when the last
        // shared_ptr referencing it goes out of scope (safe reference counting)
        current_config_ = new_config_ptr;

        std::cout << "ConfigManager: Configuration successfully reloaded" << std::endl;
        return true;

    } catch (const std::exception &e) {
        std::cerr << "ConfigManager: Failed to reload configuration: " << e.what() << std::endl;
        std::cerr << "ConfigManager: Keeping current configuration" << std::endl;
        return false;
    }
}


} // namespace cfg2
