#include "body_handler.hpp"
#include "cfg2/config.hpp"
#include "logger/logger.hpp"
#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <fstream>

namespace gwmilter {

using std::string;

pdf_body_handler::pdf_body_handler(const cfg2::PdfEncryptionSection &settings)
    : main_boundary_{generate_boundary(30)},
      pdf_attachment_{settings.pdf_attachment},
      pdf_font_path_{settings.pdf_font_path},
      pdf_font_size_{settings.pdf_font_size},
      pdf_margin_{settings.pdf_margin},
      pdf_password_{settings.pdf_password},
      pdf_main_page_if_missing_{settings.pdf_main_page_if_missing},
      email_body_replacement_{settings.email_body_replacement}
{ }


void pdf_body_handler::write(const std::string &data)
{
    body_handler_base::write(data);
    body_.write(data);
}


headers_type pdf_body_handler::get_headers()
{
    header_item h{"Content-Type", "multipart/mixed;\r\n\tboundary=\"" + main_boundary_ + "\"", 1, true};

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


void pdf_body_handler::encrypt(const recipients_type &recipients, std::string &out)
{
    using namespace epdfcrypt;
    using std::string;

    postprocess();

    body_.flush();
    mime_unpacker unpacker(body_);
    unpacker.unpack();
    string body = unpacker.body_text();

    spdlog::debug("PDF settings: pdf_font_path=\"{}\", pdf_font_size={}, pdf_margin={})", pdf_font_path_,
                  pdf_font_size_, pdf_margin_);
    epdf pdf(pdf_font_path_, true, pdf_font_size_, pdf_margin_);

    if (!pdf_password_.empty())
        pdf.set_password(pdf_password_);

    if (!body.empty()) {
        spdlog::debug("PDF body created from email; size={}", body.size());
        pdf.add_text(body);
    } else if (!pdf_main_page_if_missing_.empty()) {
        spdlog::debug("PDF body set from file (could not get from email)");
        pdf.add_text(pdf_body_handler::read_file(pdf_main_page_if_missing_));
    } else
        spdlog::debug("PDF body left empty");

    // attach the original unpacked email parts
    for (const auto &part: unpacker.parts())
        pdf.attach(part);

    // clang-format off
    out =
        "This is a multi-part message in MIME format.\r\n"
        "--" + main_boundary_ + "\r\n"
        "Content-Type: text/plain; charset=ISO-8859-1\r\n"
        "Content-Transfer-Encoding: 7bit\r\n"
        "\r\n";
    // clang-format on

    if (!email_body_replacement_.empty()) {
        spdlog::debug("email body replaced");
        out += pdf_body_handler::read_file(email_body_replacement_);
    }

    // clang-format off
    out += "\r\n\r\n"
        "--" + main_boundary_ + "\r\n"
        "Content-Type: application/pdf;\r\n"
        "   name=\"" + pdf_attachment_ + "\"\r\n"
        "Content-Transfer-Encoding: base64\r\n"
        "Content-Disposition: attachment;\r\n"
        "   filename=\"" + pdf_attachment_ + "\"\r\n\r\n";
    // clang-format on

    out += pdf.base64();

    if (out.substr(out.size() - 2) != "\r\n")
        out += "\r\n";

    out += "--" + main_boundary_ + "--\r\n";
}


void pdf_body_handler::preprocess()
{
    if (preprocessed_)
        // already done
        return;

    preprocessed_ = true;

    if (headers_.empty())
        // nothing to do
        return;

    // Content-* headers are inserted at the beginning of the new body.
    for (auto &h: headers_) {
        if (boost::iequals(h.name.substr(0, 8), "Content-")) {
            write(h.name + ": " + h.value + "\r\n");

            // mark as deleted
            h.modified = true;
            h.value.clear();
        }
    }

    write("\r\n");
}


void pdf_body_handler::postprocess()
{ /* do nothing */
}


bool pdf_body_handler::has_public_key(const std::string &recipient) const
{
    // PDF encryption doesn't use public key infrastructure.
    // Returning true ensures key_not_found_policy is never checked for PDF sections
    // (see milter_message::on_envrcpt and BaseEncryptionSection::key_not_found_policy_value).
    return true;
}


bool pdf_body_handler::import_public_key(const std::string &recipient)
{
    // NOTE: the notion of public key does not apply to PDF encryption
    return true;
}


std::string pdf_body_handler::read_file(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        throw std::runtime_error("Unable to open file: " + filename);

    std::stringstream content;
    string line;
    while (std::getline(file, line))
        content << line << "\r\n";

    return content.str();
}


} // namespace gwmilter
