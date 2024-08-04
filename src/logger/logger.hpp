#pragma once
#include "cfg/cfg.hpp"
#include <spdlog/spdlog.h>
#include <syslog.h>

namespace logger {

enum types { console = 0, syslog = 1 };

enum priorities {
    priority_trace = spdlog::level::trace,
    priority_debug = spdlog::level::debug,
    priority_info = spdlog::level::info,
    priority_warn = spdlog::level::warn,
    priority_err = spdlog::level::err,
    priority_critical = spdlog::level::critical
};

enum facilities {
    facility_user = LOG_USER,
    facility_mail = LOG_MAIL,
    facility_news = LOG_NEWS,
    facility_uucp = LOG_UUCP,
    facility_daemon = LOG_DAEMON,
    facility_auth = LOG_AUTH,
    facility_cron = LOG_CRON,
    facility_lpr = LOG_LPR,
    facility_local0 = LOG_LOCAL0,
    facility_local1 = LOG_LOCAL1,
    facility_local2 = LOG_LOCAL2,
    facility_local3 = LOG_LOCAL3,
    facility_local4 = LOG_LOCAL4,
    facility_local5 = LOG_LOCAL5,
    facility_local6 = LOG_LOCAL6,
    facility_local7 = LOG_LOCAL7
};

void init(const gwmilter::cfg::cfg &cfg);

} // end namespace logger
