# cfg2 Migration Plan: Option A Implementation

## Overview

This document outlines the complete migration from the current `cfg::cfg` singleton to the new `cfg2::ConfigManager` system using Option A: Constructor Injection with ConfigManager Reference and file-scoped global storage.

## Migration Goals

- Replace `cfg::cfg::inst()` singleton pattern with thread-safe `cfg2::ConfigManager`
- Maintain hot-reload capability for configuration changes
- Ensure zero-downtime migration path
- Preserve existing configuration file format compatibility
- Provide clean rollback mechanism

## Current Architecture Analysis

### Current cfg Usage Locations

1. **Main Application Initialization** (`src/main.cpp:102-104`):
   ```cpp
   cfg::cfg::inst().init(config_file);
   const auto g = cfg::cfg::inst().section(cfg::GENERAL_SECTION);
   logger::init(cfg::cfg::inst());
   ```

2. **Milter Message Processing** (`src/milter/milter_message.cpp`):
   - Line 54: `cfg::cfg::inst().find_match(rcpt)` - recipient matching
   - Line 187: `cfg::cfg::inst().section(cfg::GENERAL_SECTION)` - general settings
   - Line 314: `cfg::cfg::inst().section(cfg::GENERAL_SECTION)->get<>("strip_headers")` - header configuration

3. **Utility Functions** (`src/utils/dump_email.cpp:15`):
   ```cpp
   cfg::cfg::inst().section(cfg::GENERAL_SECTION)->get<bool>("dump_email_on_panic")
   ```

4. **Logger Initialization** (`src/logger/logger.cpp:9`):
   ```cpp
   void init(const gwmilter::cfg::cfg &cfg)
   ```

## Implementation Plan

### Phase 1: Infrastructure Setup

#### Step 1.1: Create Global ConfigManager Access Layer

**File**: `src/milter/config_access.hpp`
```cpp
#pragma once
#include "cfg2/config_manager.hpp"

namespace gwmilter {
    // Thread-safe access to global ConfigManager
    void set_config_manager(cfg2::ConfigManager* manager);
    cfg2::ConfigManager& get_config_manager();
    std::shared_ptr<const cfg2::Config> get_current_config();
}
```

**File**: `src/milter/config_access.cpp`
```cpp
#include "config_access.hpp"
#include <stdexcept>

namespace gwmilter {
    namespace {
        // File-scoped global - only accessible within this translation unit
        cfg2::ConfigManager* g_configManager = nullptr;
    }
    
    void set_config_manager(cfg2::ConfigManager* manager) {
        if (!manager) {
            throw std::invalid_argument("ConfigManager cannot be null");
        }
        g_configManager = manager;
    }
    
    cfg2::ConfigManager& get_config_manager() {
        if (!g_configManager) {
            throw std::runtime_error("ConfigManager not initialized - call set_config_manager() first");
        }
        return *g_configManager;
    }
    
    std::shared_ptr<const cfg2::Config> get_current_config() {
        return get_config_manager().getConfig();
    }
}
```

#### Step 1.2: Update milter_connection Class

**File**: `src/milter/milter_connection.hpp` - Add ConfigManager support:

```cpp
#pragma once
#include "logger/logger.hpp"
#include "milter_message.hpp"
#include "cfg2/config_manager.hpp" // Add this include
#include <cassert>
#include <libmilter/mfapi.h>
#include <string>

namespace gwmilter {

class milter_connection {
private:
    SMFICTX *smfictx_;
    cfg2::ConfigManager& configMgr_;              // Add ConfigManager reference
    uid_generator uid_gen_;
    std::string connection_id_;
    std::shared_ptr<milter_message> msg_;

public:
    // Update constructor to accept ConfigManager reference
    explicit milter_connection(SMFICTX *ctx, cfg2::ConfigManager& configMgr)
        : smfictx_{ctx}, configMgr_{configMgr}, connection_id_{uid_gen_.generate()}
    { }
    
    milter_connection(const milter_connection &) = delete;
    milter_connection &operator=(const milter_connection &) = delete;

    // Existing methods...
    sfsistat on_connect(const std::string &hostname, _SOCK_ADDR *hostaddr);
    sfsistat on_helo(const std::string &helohost);
    sfsistat on_close();
    sfsistat on_eom();
    sfsistat on_abort();
    sfsistat on_unknown(const std::string &arg);

    // Add config access method
    std::shared_ptr<const cfg2::Config> getConfig() const {
        return configMgr_.getConfig();
    }

    std::shared_ptr<milter_message> get_message() {
        if (msg_ == nullptr) {
            spdlog::debug("{}: get_message() creating new milter_message object", connection_id_);
            // Pass config access to milter_message
            msg_ = std::make_shared<milter_message>(smfictx_, connection_id_, configMgr_);
        }
        return msg_;
    }

private:
    static std::string hostaddr_to_string(_SOCK_ADDR *hostaddr);
};

} // end namespace gwmilter
```

#### Step 1.3: Update milter_message Class

**File**: `src/milter/milter_message.hpp` - Add ConfigManager support:

```cpp
// Add to existing includes
#include "cfg2/config_manager.hpp"

class milter_message {
private:
    cfg2::ConfigManager& configMgr_;  // Add ConfigManager reference
    
public:
    // Update constructor
    explicit milter_message(SMFICTX *ctx, const std::string &connection_id, cfg2::ConfigManager& configMgr);
    
    // Add config access method
    std::shared_ptr<const cfg2::Config> getConfig() const {
        return configMgr_.getConfig();
    }
    
    // Existing methods remain the same...
};
```

### Phase 2: Update Callback Integration

#### Step 2.1: Update milter_callbacks.cpp

**File**: `src/milter/milter_callbacks.cpp`:

```cpp
#include "milter_callbacks.hpp"
#include "logger/logger.hpp"
#include "milter_connection.hpp"
#include "config_access.hpp"  // Add this include
#include <libmilter/mfapi.h>
#include <string>
#include <vector>

namespace gwmilter {

sfsistat xxfi_connect(SMFICTX *ctx, char *hostname, _SOCK_ADDR *hostaddr)
{
    try {
        // Get ConfigManager reference from global access layer
        cfg2::ConfigManager& configMgr = get_config_manager();
        
        // Create milter_connection with ConfigManager reference
        auto *m = new milter_connection(ctx, configMgr);
        smfi_setpriv(ctx, m);

        return m->on_connect(hostname, hostaddr);
    } catch (const std::exception &e) {
        spdlog::error("std::exception caught: {}", e.what());
    } catch (...) {
        spdlog::error("unknown exception caught");
    }

    return SMFIS_TEMPFAIL;
}

// Other callbacks remain unchanged - they use smfi_getpriv(ctx) to get milter_connection*

} // namespace gwmilter
```

### Phase 3: Replace cfg Usage

#### Step 3.1: Update main.cpp

**File**: `src/main.cpp`:

```cpp
// Add cfg2 includes
#include "cfg2/config_manager.hpp"
#include "milter/config_access.hpp"

// Replace existing cfg initialization (around line 102)
int main(int argc, char *argv[]) {
    // ... existing argument parsing ...
    
    try {
        // seed random number generator
        srand(time(nullptr));

        // PHASE 3 MIGRATION: Initialize cfg2 instead of cfg
        cfg2::ConfigManager configManager(config_file);
        
        // Make ConfigManager available to milter callbacks
        gwmilter::set_config_manager(&configManager);
        
        // Get config for main() usage
        auto config = configManager.getConfig();
        
        // Initialize logger with cfg2
        logger::init_cfg2(config->general);
        
        if (config->general.daemonize) {
            if (daemon(0, 0) == -1) {
                cerr << "daemon() call failed: " << utils::string::str_err(errno);
                return EXIT_FAILURE;
            }
        }

        drop_privileges(config->general.user, config->general.group);

        spdlog::info("gwmilter starting");
        gwmilter::milter m(config->general.milter_socket,
                           SMFIF_ADDHDRS | SMFIF_CHGHDRS | SMFIF_CHGBODY | SMFIF_ADDRCPT | SMFIF_ADDRCPT_PAR |
                               SMFIF_DELRCPT | SMFIF_QUARANTINE | SMFIF_CHGFROM | SMFIF_SETSYMLIST,
                           config->general.milter_timeout);

        m.run();
        
        spdlog::info("gwmilter shutting down");
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        spdlog::error("Exception caught: {}", e.what());
    } catch (...) {
        spdlog::error("Unknown exception caught");
    }

    return EXIT_FAILURE;
}
```

#### Step 3.2: Update milter_message.cpp

**File**: `src/milter/milter_message.cpp` - Replace cfg usage:

```cpp
// Constructor update
milter_message::milter_message(SMFICTX *ctx, const std::string &connection_id, cfg2::ConfigManager& configMgr)
    : smfictx_{ctx}, configMgr_{configMgr}, message_id_{connection_id}
{
}

// Update recipient processing (around line 54)
sfsistat milter_message::on_envrcpt(const std::vector<std::string> &args) {
    const std::string &rcpt = args[0];
    spdlog::info("{}: to={}", message_id_, rcpt);
    
    // REPLACE: cfg::cfg::inst().find_match(rcpt)
    auto config = getConfig();
    const cfg2::BaseEncryptionSection* encSection = config->find_match(rcpt);
    
    if (encSection == nullptr) {
        spdlog::debug("{}: no encryption configuration found for {}", message_id_, rcpt);
        return SMFIS_CONTINUE;
    }
    
    // Convert cfg2::BaseEncryptionSection to cfg::encryption_section_handler equivalent
    // This may require adaptation layer or direct cfg2 usage
    
    // Continue with existing logic...
}

// Update EOM processing (around line 187)
sfsistat milter_message::on_eom() {
    try {
        auto config = getConfig();
        // REPLACE: cfg::cfg::inst().section(cfg::GENERAL_SECTION)
        const cfg2::GeneralSection& general = config->general;
        
        // Continue with existing logic using general.smtp_server, etc.
        
    } catch (...) {
        // Handle errors
    }
}

// Update strip headers (around line 314)
void milter_message::strip_configured_headers() {
    auto config = getConfig();
    // REPLACE: cfg::cfg::inst().section(cfg::GENERAL_SECTION)->get<>("strip_headers")
    const std::vector<std::string>& strip_headers = config->general.strip_headers;
    
    for (const auto &header: strip_headers) {
        // Existing logic...
    }
}
```

#### Step 3.3: Update Logger Integration

**File**: `src/logger/logger.hpp` - Add cfg2 support:

```cpp
#include "cfg2/config.hpp"  // Add this

namespace logger {
    // Existing function for backward compatibility
    void init(const gwmilter::cfg::cfg &cfg);
    
    // New function for cfg2
    void init_cfg2(const cfg2::GeneralSection& general);
}
```

**File**: `src/logger/logger.cpp` - Add cfg2 implementation:

```cpp
void init_cfg2(const cfg2::GeneralSection& general) {
    auto type = static_cast<types>(general.log_type);
    auto priority = static_cast<priorities>(general.log_priority);

    switch (type) {
    case console:
        // noop: use the default logger.
        break;
    case syslog:
        auto facility = static_cast<facilities>(general.log_facility);
        auto syslog_logger = spdlog::syslog_logger_mt("syslog", "gwmilter", LOG_PID, facility);
        spdlog::set_default_logger(syslog_logger);
    }

    spdlog::set_level(static_cast<spdlog::level::level_enum>(priority));
}
```

#### Step 3.4: Update Utility Functions

**File**: `src/utils/dump_email.cpp`:

```cpp
#include "milter/config_access.hpp"  // Add this include

void dump_email_if_enabled(const std::string &message_content) {
    // REPLACE: cfg::cfg::inst().section(cfg::GENERAL_SECTION)->get<bool>("dump_email_on_panic")
    auto config = gwmilter::get_current_config();
    if (!config->general.dump_email_on_panic) {
        return;
    }
    
    // Existing logic...
}
```

### Phase 4: Configuration Mapping

#### Step 4.1: Ensure cfg2 Structure Compatibility

Verify that `cfg2::GeneralSection` and `cfg2::BaseEncryptionSection` have all fields needed:

**Required GeneralSection fields:**
- `milter_socket` (string)
- `daemonize` (bool) 
- `user` (string)
- `group` (string)
- `log_type` (int)
- `log_facility` (int)
- `log_priority` (int)
- `milter_timeout` (int)
- `smtp_server` (string)
- `smtp_server_timeout` (int)
- `dump_email_on_panic` (bool)
- `signing_key` (string)
- `strip_headers` (vector<string>)

**Required BaseEncryptionSection fields:**
- `encryption_protocol` (enum)
- Pattern matching capability equivalent to `find_match()`

### Phase 5: Testing Strategy

#### Step 5.1: Unit Tests

Create comprehensive tests in `src/cfg2/migration_tests.cpp`:

```cpp
#include <gtest/gtest.h>
#include "cfg2/config_manager.hpp"
#include "milter/config_access.hpp"

class MigrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test configuration
        configManager = std::make_unique<cfg2::ConfigManager>("testdata/test_config.ini");
        gwmilter::set_config_manager(configManager.get());
    }
    
    void TearDown() override {
        // Cleanup
    }
    
    std::unique_ptr<cfg2::ConfigManager> configManager;
};

TEST_F(MigrationTest, ConfigManagerAccessWorks) {
    auto& mgr = gwmilter::get_config_manager();
    auto config = mgr.getConfig();
    ASSERT_NE(config, nullptr);
}

TEST_F(MigrationTest, ConfigContainsRequiredFields) {
    auto config = gwmilter::get_current_config();
    
    // Test general section
    EXPECT_FALSE(config->general.milter_socket.empty());
    EXPECT_GE(config->general.milter_timeout, 0);
    
    // Test encryption sections
    auto* section = config->find_match("test@example.com");
    EXPECT_NE(section, nullptr);
}

TEST_F(MigrationTest, HotReloadWorks) {
    auto config1 = gwmilter::get_current_config();
    
    // Modify config file
    // ... modify test config file ...
    
    EXPECT_TRUE(gwmilter::get_config_manager().reload());
    
    auto config2 = gwmilter::get_current_config();
    EXPECT_NE(config1.get(), config2.get());  // Different objects
}
```

#### Step 5.2: Integration Tests

Create tests that verify milter_connection creation and config access:

```cpp
TEST_F(MigrationTest, MilterConnectionCreation) {
    // Mock SMFICTX
    SMFICTX mockCtx;
    
    // This should not throw
    EXPECT_NO_THROW({
        gwmilter::milter_connection conn(&mockCtx, gwmilter::get_config_manager());
        auto config = conn.getConfig();
        EXPECT_NE(config, nullptr);
    });
}
```

### Phase 6: Deployment Strategy

#### Step 6.1: Feature Flag Approach

Add compile-time feature flag to enable gradual migration:

```cpp
// In CMakeLists.txt
option(ENABLE_CFG2 "Enable cfg2 configuration system" OFF)

// In code
#ifdef ENABLE_CFG2
    // Use cfg2 system
    cfg2::ConfigManager configManager(config_file);
    // ...
#else  
    // Use existing cfg system
    cfg::cfg::inst().init(config_file);
    // ...
#endif
```

#### Step 6.2: Validation Testing

1. **Build with cfg2 disabled** - ensure no regressions
2. **Build with cfg2 enabled** - verify functionality
3. **Run integration tests** against both systems
4. **Performance testing** - compare config access overhead

### Phase 7: Rollback Plan

#### Step 7.1: Rollback Mechanism

If issues arise, rollback can be achieved by:

1. **Immediate**: Revert to previous git commit
2. **Configuration**: Change `ENABLE_CFG2=OFF` and rebuild
3. **Runtime**: No runtime toggle needed - uses compile-time feature flag

#### Step 7.2: Rollback Testing

Verify rollback works by:
1. Migrating to cfg2
2. Running tests
3. Rolling back to cfg 
4. Verifying all tests still pass

## Risk Assessment

### High Risk Items

1. **Configuration file compatibility** - cfg2 must read existing config files
2. **Performance impact** - ConfigManager access overhead
3. **Thread safety** - Multiple SMTP connections accessing config simultaneously
4. **Hot reload behavior** - Mid-flight connections using different configs

### Mitigation Strategies

1. **Extensive testing** with existing production config files
2. **Benchmarking** config access performance  
3. **Staged rollout** with feature flags
4. **Monitoring** for config-related errors post-deployment

## Success Criteria

- [ ] All existing `cfg::cfg::inst()` calls replaced with `cfg2` equivalents
- [ ] Unit tests pass with 100% coverage for config access paths
- [ ] Integration tests pass with real config files
- [ ] Performance regression < 5% for config access operations
- [ ] Hot reload functionality working
- [ ] Clean rollback capability demonstrated

## Post-Migration Cleanup

After successful migration:

1. Remove old `cfg` system code
2. Remove feature flags
3. Update documentation
4. Remove backward compatibility layers
5. Optimize cfg2 performance if needed

## Appendix: API Mapping Reference

| Old cfg API | New cfg2 API |
|-------------|--------------|
| `cfg::cfg::inst().init(file)` | `cfg2::ConfigManager(file)` |
| `cfg::cfg::inst().section(GENERAL)` | `config->general` |
| `cfg::cfg::inst().find_match(rcpt)` | `config->find_match(rcpt)` |
| `section->get<T>("field")` | `section.field` |
| `cfg::cfg::inst()` (in callbacks) | `gwmilter::get_config_manager()` |

This migration plan provides a complete roadmap for replacing the singleton cfg system with the thread-safe cfg2 ConfigManager while maintaining backward compatibility and providing clear rollback options.