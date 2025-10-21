#include "logger/spdlog_init.hpp"

#include "cfg2/config.hpp"
#include "logger/logger.hpp"
#include <map>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <stdexcept>
#include <string>
#include <syslog.h>

namespace gwmilter::logging {
namespace {
constexpr char SYSLOG_LOGGER_NAME[] = "gwmilter_syslog";
constexpr char CONSOLE_LOGGER_NAME[] = "gwmilter_console";
} // namespace

void init_spdlog(const cfg2::GeneralSection &general_section)
{
    static const std::map<std::string, spdlog::level::level_enum> priority_map = {
        {"trace", spdlog::level::trace},  {"debug", spdlog::level::debug}, {"info", spdlog::level::info},
        {"warning", spdlog::level::warn}, {"error", spdlog::level::err},   {"critical", spdlog::level::critical},
    };

    static const std::map<std::string, int> facility_map = {
        {"user", LOG_USER},     {"mail", LOG_MAIL},     {"news", LOG_NEWS},     {"uucp", LOG_UUCP},
        {"daemon", LOG_DAEMON}, {"auth", LOG_AUTH},     {"cron", LOG_CRON},     {"lpr", LOG_LPR},
        {"local0", LOG_LOCAL0}, {"local1", LOG_LOCAL1}, {"local2", LOG_LOCAL2}, {"local3", LOG_LOCAL3},
        {"local4", LOG_LOCAL4}, {"local5", LOG_LOCAL5}, {"local6", LOG_LOCAL6}, {"local7", LOG_LOCAL7},
    };

    if (general_section.log_type == "syslog") {
        const auto facility_it = facility_map.find(general_section.log_facility);
        if (facility_it == facility_map.end())
            throw std::runtime_error("Invalid log_facility: " + general_section.log_facility);

        spdlog::drop(CONSOLE_LOGGER_NAME);
        spdlog::drop(SYSLOG_LOGGER_NAME);
        auto syslog_logger = spdlog::syslog_logger_mt(SYSLOG_LOGGER_NAME, "gwmilter", LOG_PID, facility_it->second);
        spdlog::set_default_logger(syslog_logger);
    } else {
        spdlog::drop(SYSLOG_LOGGER_NAME);
        auto console_logger = spdlog::get(CONSOLE_LOGGER_NAME);
        if (!console_logger)
            console_logger = spdlog::stdout_color_mt(CONSOLE_LOGGER_NAME);
        spdlog::set_default_logger(console_logger);
    }

    const auto priority_it = priority_map.find(general_section.log_priority);
    if (priority_it == priority_map.end())
        throw std::runtime_error("Invalid log_priority: " + general_section.log_priority);

    spdlog::set_level(priority_it->second);
}

} // namespace gwmilter::logging
