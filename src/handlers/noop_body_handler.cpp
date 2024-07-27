#include "body_handler.hpp"
#include <string>

namespace gwmilter {

noop_body_handler::noop_body_handler(const std::shared_ptr<cfg::encryption_section_handler> &)
{ }


void noop_body_handler::write(const std::string &data)
{
    data_ += data;
}


headers_type noop_body_handler::get_headers()
{
    return headers_;
}


void noop_body_handler::encrypt(const std::set<std::string> &, std::string &out)
{
    out.swap(data_);
}


bool noop_body_handler::has_public_key(const std::string &) const
{
    return true;
}


bool noop_body_handler::import_public_key(const std::string &)
{
    return true;
}

} // namespace gwmilter
