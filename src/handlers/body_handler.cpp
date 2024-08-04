#include "body_handler.hpp"
#include "logger/logger.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace gwmilter {

body_handler_base::body_handler_base()
    : preprocessed_{false}
{ }


void body_handler_base::add_header(const std::string &headerf, const std::string &headerv)
{
    headers_.emplace_back(headerf, headerv, ++header_pos_[headerf], false);
}


void body_handler_base::write(const std::string &)
{
    preprocess();
}


void body_handler_base::preprocess()
{
    if (preprocessed_)
        // already done
        return;

    preprocessed_ = true;

    if (headers_.empty())
        // nothing to do
        return;

    // Content-* headers are inserted at the beginning of the new body.
    headers_type content_headers;
    std::string content_type = extract_content_headers(content_headers);

    if (convert_to_multipart())
        make_multipart(content_type);

    // append Content-* headers to body
    for (const auto &h: content_headers)
        write(h.name + ": " + h.value + "\r\n");

    write("\r\n");
}


void body_handler_base::postprocess()
{
    if (!multipart_boundary_.empty())
        // end the message properly
        write("\r\n--" + multipart_boundary_ + "--\r\n");
}


std::string body_handler_base::generate_boundary(std::size_t length)
{
    static std::string allowed_chars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");
    std::string boundary;
    boundary.resize(length);

    for (unsigned int i = 0; i < length; ++i)
        boundary[i] = allowed_chars[rand() % allowed_chars.size()];

    return boundary;
}


std::string body_handler_base::extract_content_headers(headers_type &content_headers)
{
    std::string content_type;
    for (auto &h: headers_) {
        if (boost::iequals(h.name.substr(0, 8), "Content-")) {
            if (boost::iequals(h.name, "Content-Type"))
                content_type = boost::algorithm::to_lower_copy(h.value);

            header_item header{h};
            // mark as deleted
            h.modified = true;
            h.value.clear();

            content_headers.push_back(header);
        }
    }

    return content_type;
}


void body_handler_base::make_multipart(const std::string &content_type)
{
    if (!boost::iequals(content_type.substr(0, 10), "multipart/")) {
        // NOTE: this is a special case.
        // If Content-Type is not multipart/*, wrap the body
        // in multipart/mixed.
        multipart_boundary_ = generate_boundary(30);
        // clang-format off
        write(
                "Content-Type: multipart/mixed;\r\n"
                "\tboundary=\"" + multipart_boundary_ + "\"\r\n"
                "\r\n"
                "--" + multipart_boundary_ + "\r\n");
        // clang-format on

        spdlog::debug("Content converted to multipart/mixed");
    }
}


bool body_handler_base::convert_to_multipart() const
{
    return false;
}


egpgcrypt_body_handler::egpgcrypt_body_handler(gpgme_protocol_t protocol)
    : crypto_{protocol}
{ }


void egpgcrypt_body_handler::write(const std::string &data)
{
    body_handler_base::write(data);
    body_.write(data);
}


bool egpgcrypt_body_handler::has_public_key(const std::string &recipient) const
{
    return crypto_.has_public_key(recipient);
}


bool egpgcrypt_body_handler::import_public_key(const std::string &recipient)
{
    return crypto_.import_public_key(recipient);
}

} // namespace gwmilter
