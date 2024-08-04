#include "logger.hpp"
#include "cfg/cfg.hpp"
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/spdlog.h>

namespace logger {

void init(const gwmilter::cfg::cfg &cfg)
{
    const auto g = cfg.section(gwmilter::cfg::GENERAL_SECTION);
    auto type = static_cast<types>(g->get<int>("log_type"));
    auto priority = static_cast<priorities>(g->get<int>("log_priority"));

    switch (type) {
    case console:
        // noop: use the default logger.
        // Per spdlog docs the default is stdout, multithreaded, colored.
        break;
    case syslog:
        auto facility = static_cast<facilities>(g->get<int>("log_facility"));
        auto syslog_logger = spdlog::syslog_logger_mt("syslog", "gwmilter", LOG_PID, facility);
        spdlog::set_default_logger(syslog_logger);
    }

    spdlog::set_level(static_cast<spdlog::level::level_enum>(priority));
}

} // namespace logger
