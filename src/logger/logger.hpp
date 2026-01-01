#pragma once

// Temporary compatibility shim while migrating off the legacy logger module.
// Keeps the include path stable and exposes spdlog to existing call sites.

#include <spdlog/spdlog.h>
