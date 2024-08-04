#include "body_handler.hpp"
#include "logger/logger.hpp"
#include "utils/string.hpp"
#include <boost/algorithm/string/predicate.hpp>

namespace gwmilter {

pgp_body_handler::pgp_body_handler()
    : egpgcrypt_body_handler{GPGME_PROTOCOL_OpenPGP}, main_boundary_{generate_boundary(30)}
{ }


headers_type pgp_body_handler::get_headers()
{
    // clang-format off
    header_item h{"Content-Type",
                "multipart/encrypted;\r\n"
                "\tprotocol=\"application/pgp-encrypted\";\r\n"
                "\tboundary=\"" + main_boundary_ + "\"",
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

    return headers_;
}


void pgp_body_handler::encrypt(const std::set<std::string> &recipients, std::string &out)
{
    using namespace egpgcrypt;

    // the body is complete, do the necessary post-processing
    postprocess();

    // Prepare body according to RFC 3156
    // clang-format off
    out.append(
            "--" + main_boundary_ + "\r\n"
            "Content-Type: application/pgp-encrypted\r\n"
            "\r\n"
            "Version: 1\r\n"
            "\r\n"
            "--" + main_boundary_ + "\r\n"
            "Content-Type: application/octet-stream\r\n"
            "\r\n");
    // clang-format on

    // encrypt
    memory_data_buffer encrypted_body;
    body_.seek(0, data_buffer::SET);
    crypto_.encrypt(recipients, expired_keys_, body_, encrypted_body);

    if (!expired_keys_.empty())
        spdlog::warn("Following PGP keys have expired: {}", utils::string::set_to_string(expired_keys_));

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

    // end MIME
    out.append("\r\n--" + main_boundary_ + "--\r\n");
}


bool pgp_body_handler::convert_to_multipart() const
{
    return true;
}

} // namespace gwmilter
