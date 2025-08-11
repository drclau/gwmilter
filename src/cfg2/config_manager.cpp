#include "config_manager.hpp"
#include "ini_reader.hpp"
#include <filesystem>
#include <iostream>

namespace cfg2 {

ConfigManager::ConfigManager(const std::string &config_file)
{
    initialize(config_file);
}

void ConfigManager::initialize(const std::string &config_file)
{
    std::lock_guard<std::mutex> lock(config_mutex_);

    config_file_path_ = config_file;

    try {
        // Load and parse the configuration
        ConfigNode root = parseIniFile(config_file_path_);
        auto new_config_ptr = std::make_shared<Config>(deserialize<Config>(root));

        // Atomically store the new shared_ptr
        std::atomic_store(&current_config_, new_config_ptr);

        std::cout << "ConfigManager: Successfully loaded configuration from " << config_file << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "ConfigManager: Failed to initialize with config file '" << config_file << "': " << e.what()
                  << std::endl;
        throw;
    }
}

std::shared_ptr<const Config> ConfigManager::getConfig() const
{
    // Atomically load the shared_ptr - this ensures proper reference counting
    // The Config object will stay alive as long as any shared_ptr references it
    return std::atomic_load(&current_config_);
}

bool ConfigManager::reload()
{
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (config_file_path_.empty()) {
        std::cerr << "ConfigManager: Cannot reload - not initialized with config file" << std::endl;
        return false;
    }

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

        // Atomically store the new shared_ptr
        // The old Config object will be automatically destroyed when the last
        // shared_ptr referencing it goes out of scope (safe reference counting)
        std::atomic_store(&current_config_, new_config_ptr);

        std::cout << "ConfigManager: Configuration successfully reloaded" << std::endl;
        return true;

    } catch (const std::exception &e) {
        std::cerr << "ConfigManager: Failed to reload configuration: " << e.what() << std::endl;
        std::cerr << "ConfigManager: Keeping current configuration" << std::endl;
        return false;
    }
}

bool ConfigManager::isInitialized() const
{
    return std::atomic_load(&current_config_) != nullptr;
}

ConfigManager::~ConfigManager()
{
    // The shared_ptr will automatically clean up when it goes out of scope
    // No manual memory management needed
}

} // namespace cfg2