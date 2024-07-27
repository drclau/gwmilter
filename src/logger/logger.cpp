#include "logger.hpp"
#include <iostream>

namespace logger {

thread_local std::string syslog_writer::buf_;


syslog_writer::syslog_writer(const std::string &ident, facilities facility, priorities priority, int logopt)
    : ident_{ident}, default_priority_{priority}, priority_{priority}
{
    openlog(ident.c_str(), logopt, facility);
    // log all levels up to and including logLevel
    setlogmask(LOG_UPTO(priority));
}


syslog_writer::int_type syslog_writer::sync()
{
    if (buf_.empty())
        return 0;

    // write buffer to syslog
    syslog(priority_, "%s", buf_.c_str());
    buf_.clear();

    // reset priority to default
    priority_ = default_priority_;

    // syslog() always succeeds...
    return 0;
}


syslog_writer::int_type syslog_writer::overflow(int_type ch)
{
    if (traits_type::eq_int_type(ch, traits_type::eof()))
        sync();
    else
        buf_.push_back(traits_type::to_char_type(ch));

    return ch;
}

} // namespace logger
