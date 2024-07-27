#pragma once
#include <iostream>
#include <streambuf>
#include <syslog.h>

namespace logger {

/**
 *  Priorities, per POSIX.1-2001
 *  http://pubs.opengroup.org/onlinepubs/9699919799/
 */
enum priorities {
    priority_emerg = LOG_EMERG,
    priority_alert = LOG_ALERT,
    priority_crit = LOG_CRIT,
    priority_err = LOG_ERR,
    priority_warning = LOG_WARNING,
    priority_notice = LOG_NOTICE,
    priority_info = LOG_INFO,
    priority_debug = LOG_DEBUG
};


/**
 *  Facilities, per POSIX.1-2001
 *  (LOG_KERN is excluded because it's reserved)
 *  http://pubs.opengroup.org/onlinepubs/9699919799/
 */
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


class syslog_writer : public std::basic_streambuf<char> {
public:
    syslog_writer(const std::string &ident, facilities facility, priorities priority, int logopt = LOG_PID);
    syslog_writer(const syslog_writer &) = delete;
    syslog_writer &operator=(const syslog_writer &) = delete;

    priorities set_priority(priorities new_priority)
    {
        priorities old = priority_;
        priority_ = new_priority;
        return old;
    }

protected:
    int_type sync() override;
    int_type overflow(int_type ch) override;

private:
    static thread_local std::string buf_;
    const std::string ident_;
    const priorities default_priority_;
    priorities priority_;
};


class logger {
public:
    std::ostream &get_stream(priorities priority, const char *file, unsigned int line, const char *func)
    {
        auto writer = dynamic_cast<syslog_writer *>(std::clog.rdbuf());
        if (writer != nullptr)
            writer->set_priority(priority);

        return std::clog;
    }

    ~logger() { std::clog << std::endl; }
};


#define L_DEBUG logger::logger().get_stream(logger::priority_debug, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define L_INFO logger::logger().get_stream(logger::priority_info, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define L_NOTICE logger::logger().get_stream(logger::priority_notice, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define L_WARN logger::logger().get_stream(logger::priority_warning, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define L_ERR logger::logger().get_stream(logger::priority_err, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define L_CRIT logger::logger().get_stream(logger::priority_crit, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define L_EMERG logger::logger().get_stream(logger::priority_emerg, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define L_ALERT logger::logger().get_stream(logger::priority_alert, __FILE__, __LINE__, __PRETTY_FUNCTION__)

} // end namespace logger
