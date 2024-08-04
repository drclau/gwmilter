#include "body_handler.hpp"
#include "logger/logger.hpp"
#include "utils/string.hpp"
#include <boost/algorithm/string/predicate.hpp>

namespace gwmilter {

smime_body_handler::smime_body_handler()
    : egpgcrypt_body_handler{GPGME_PROTOCOL_CMS}, new_headers_added_{false}
{ }


headers_type smime_body_handler::get_headers()
{
    // clang-format off
    header_item h{"Content-Type",
                  "application/pkcs7-mime;\r\n"
                  "\tname=\"smime.p7m\";\r\n"
                  "\tsmime-type=enveloped-data",
                  1, true};
    // clang-format on

    if (auto it = std::find_if(headers_.begin(), headers_.end(),
                               [&h](const header_item &item) { return boost::iequals(item.name, h.name); });
        it != headers_.end())
    {
        // Update the existing Content-Type header
        *it = std::move(h);
    } else {
        // Content-Type header not found, hence creating it
        headers_.emplace_back(std::move(h));
    }

    if (!new_headers_added_) {
        new_headers_added_ = true;

        headers_.emplace_back("Content-Transfer-Encoding", "base64", 1, true);
        headers_.emplace_back("Content-Disposition", "attachment; filename=\"smime.p7m\"", 1, true);
        headers_.emplace_back("Content-Description", "S/MIME Encrypted Message", 1, true);
    }

    return headers_;
}


void smime_body_handler::encrypt(const std::set<std::string> &recipients, std::string &out)
{
    using namespace egpgcrypt;

    // the body is complete, do the necessary post-processing
    postprocess();

    // encrypt
    memory_data_buffer encrypted_body;
    body_.seek(0, data_buffer::SET);
    crypto_.encrypt(recipients, expired_keys_, body_, encrypted_body);

    if (!expired_keys_.empty())
        spdlog::warn("Following S/MIME keys have expired: {}", utils::string::set_to_string(expired_keys_));

    // get encrypted data
    encrypted_body.seek(0, data_buffer::SET);
    std::string tmpbuf;
    while (encrypted_body.read(tmpbuf)) {
        // insert \r before \n
        std::string::size_type pos = 0;
        while (pos < tmpbuf.size() && (pos = tmpbuf.find('\n', pos)) != std::string::npos) {
            tmpbuf.insert(pos, 1, '\r');
            pos += 2;
        }

        out += tmpbuf;
    }
}

} // namespace gwmilter
