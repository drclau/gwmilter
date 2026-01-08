#include "body_handler.hpp"
#include <string>

namespace gwmilter {


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
    // NONE encryption is a pass-through that doesn't use public key infrastructure.
    // Returning true ensures key_not_found_policy is never checked for NONE sections
    // (see milter_message::on_envrcpt and BaseEncryptionSection::key_not_found_policy_value).
    return true;
}


bool noop_body_handler::import_public_key(const std::string &)
{
    return true;
}

} // namespace gwmilter
