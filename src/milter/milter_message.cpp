#include "milter_message.hpp"
#include "cfg/cfg.hpp"
#include "cfg2/config.hpp"
#include "handlers/body_handler.hpp"
#include "logger/logger.hpp"
#include "milter_exception.hpp"
#include "smtp/smtp_client.hpp"
#include "utils/dump_email.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <cassert>
#include <libmilter/mfapi.h>
#include <memory>
#include <string>
#include <vector>

namespace gwmilter {

const std::string milter_message::x_gwmilter_signature = "X-GWMilter-Signature";

milter_message::milter_message(SMFICTX *ctx, const std::string &connection_id,
                               std::shared_ptr<const cfg2::Config> config)
    : smfictx_{ctx}, config_{std::move(config)}, connection_id_{connection_id}, message_id_{uid_gen_.generate()}
{
    assert(config_ != nullptr && "milter_message requires non-null config");
    spdlog::info("{}: begin message (connection_id={})", message_id_, connection_id_);
}


milter_message::~milter_message()
{
    spdlog::info("{}: end message (connection_id={})", message_id_, connection_id_);
}


sfsistat milter_message::on_envfrom(const std::vector<std::string> &args)
{
    if (args.empty()) {
        spdlog::warn("{}: sender is empty", message_id_);
        return SMFIS_CONTINUE;
    }

    spdlog::info("{}: from={}", message_id_, args[0]);
    sender_ = args[0];
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_envrcpt(const std::vector<std::string> &args)
{
    if (args.empty()) {
        spdlog::error("{}: recipient is empty", message_id_);
        return SMFIS_REJECT;
    }

    const std::string &rcpt = args[0];
    spdlog::info("{}: to={}", message_id_, rcpt);

    // Look up encryption section in cfg2
    const cfg2::BaseEncryptionSection *section = config_->find_match(rcpt);

    if (section == nullptr) {
        // recipient doesn't match any section, hence reject it
        set_reply("550", "5.7.1", "recipient does not match any configuration section");
        spdlog::warn("{}: recipient {} was not found in any section, rejecting", message_id_, rcpt);
        return SMFIS_REJECT;
    }

    spdlog::debug("{}: recipient {} was found in section {}", message_id_, rcpt, section->sectionName);
    email_context &context = get_context(section);

    if (context.body_handler->has_public_key(rcpt)) {
        spdlog::debug("{}: found public key in local keyring for {}", message_id_, rcpt);
        context.recipients[rcpt] = true;
    } else {
        spdlog::debug("{}: couldn't find public key in local keyring for {}", message_id_, rcpt);

        // Get key_not_found_policy from cfg2 section (only pgp/smime expose a value).
        const auto policy = section->key_not_found_policy_value();
        if (!policy.has_value()) {
            spdlog::error("{}: section {} missing key_not_found_policy for recipient {}", message_id_,
                          section->sectionName, rcpt);
            set_reply("451", "4.3.0", "Temporary configuration error");
            return SMFIS_TEMPFAIL;
        }
        switch (*policy) {
        case cfg2::KeyNotFoundPolicy::Discard:
            spdlog::warn("{}: discarding recipient {}", message_id_, rcpt);
            context.recipients[rcpt] = false;
            break;
        case cfg2::KeyNotFoundPolicy::Retrieve:
            // XXX: maybe the key importing should be done in another place to
            // avoid delays or timeouts in this part of the MTA-to-MTA communication
            if (context.body_handler->import_public_key(rcpt)) {
                spdlog::info("{}: imported new public key for {}", message_id_, rcpt);
                context.recipients[rcpt] = true;
            } else {
                spdlog::warn("{}: failed to import new public key for {}", message_id_, rcpt);
                context.recipients[rcpt] = false;
            }
            break;
        case cfg2::KeyNotFoundPolicy::Reject:
            set_reply("550", "5.7.1", "Recipient does not have a public key");
            spdlog::warn("{}: rejected recipient {} due to missing public key", message_id_, rcpt);
            return SMFIS_REJECT;
        }
        // Empty policy means key management doesn't apply (PDF/NONE sections).
        // This path is unreachable since their handlers always return true from has_public_key().
    }

    recipients_all_.emplace_back(rcpt);
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_data()
{
    spdlog::debug("{}: data", message_id_);

    unsigned int rcpt_count = 0;
    for (auto &[_, context]: contexts_) {
        // put recipients that have public key in good_recipients
        for (const auto &[recipient, keyPresent]: context.recipients) {
            if (context.body_handler && keyPresent) {
                context.good_recipients.insert(recipient);
                ++rcpt_count;
            }
        }
    }

    if (rcpt_count == 0) {
        spdlog::warn("{}: no recipient matches the existing configuration sections, rejecting email", message_id_);
        return SMFIS_REJECT;
    }

    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_header(const std::string &headerf, const std::string &headerv)
{
    spdlog::debug("{}: header {}={}", message_id_, headerf, headerv);

    // XXX: debugging
    headers_ += headerf + ": " + headerv + "\r\n";

    if (headerf == x_gwmilter_signature) {
        signature_header_ = headerv;
        unpack_header_value(signature_header_);
        // no need to store it in protocol specific information
        return SMFIS_CONTINUE;
    }

    for (auto &[_, context]: contexts_)
        context.body_handler->add_header(headerf, headerv);

    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_eoh()
{
    spdlog::debug("{}: end-of-headers", message_id_);
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_body(const std::string &body)
{
    spdlog::debug("{}: body size={}", message_id_, body.size());
    body_ += body;
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_eom()
{
    spdlog::debug("{}: end-of-message", message_id_);

    utils::dump_email dmp("dump", "crash-", connection_id_, message_id_, headers_, body_, true,
                          config_->general.dump_email_on_panic);

    try {
        if (!signature_header_.empty()) {
            if (verify_signature()) {
                spdlog::info("{}: signature header verifies, allowing email to pass", message_id_);
                return SMFIS_CONTINUE;
            } else {
                spdlog::error("{}: rejecting email due to failure while verifying the signature", message_id_);
                set_reply("550", nullptr, "failed to verify the signature");
                return SMFIS_REJECT;
            }
        }
    } catch (const std::exception &e) {
        // TODO: std::runtime_error should be replaced with something more specific
        spdlog::error("{}: rejecting email due to failure while verifying the signature: {}", message_id_, e.what());
        set_reply("550", nullptr, "failed to verify the signature");
        return SMFIS_REJECT;
    }

    try {
        // process all matching configuration sections for current milter message
        std::vector<smtp::work_item> smtp_work_items;

        bool milter_body_replaced = false;
        for (auto &[section, ctx]: contexts_) {
            spdlog::debug("{}: processing section {}", message_id_, section);

            if (ctx.good_recipients.empty()) {
                spdlog::debug("{}: section {} has no recipients left", message_id_, section);
                continue;
            }

            ctx.body_handler->write(body_);
            ctx.body_handler->encrypt(ctx.good_recipients, *ctx.encrypted_body);

            int i = 1;
            for (const auto &r: ctx.body_handler->failed_recipients()) {
                spdlog::debug("{}: failed key #{} = {}", message_id_, i, r);
                ++i;
            }

            headers_type headers = ctx.body_handler->get_headers();

            if (!milter_body_replaced) {
                // If multiple protocols are used to encrypt this email then
                // only the first one is used to modify the email in milter.
                milter_body_replaced = true;

                // process and replace headers
                replace_headers(headers);

                // replace body, for one protocol only
                // XXX: does it make a copy of the buffer?
                if (smfi_replacebody(smfictx_,
                                     reinterpret_cast<unsigned char *>(const_cast<char *>(ctx.encrypted_body->c_str())),
                                     ctx.encrypted_body->size()) == MI_FAILURE)
                {
                    return SMFIS_TEMPFAIL;
                }

                update_milter_recipients(ctx.good_recipients);
            } else {
                // only one key is used to sign
                std::set<std::string> keys;
                keys.insert(config_->general.signing_key);
                std::string signature;
                sign(keys, *ctx.encrypted_body, signature);
                pack_header_value(signature, x_gwmilter_signature.size());

                headers.emplace_back(x_gwmilter_signature, signature, 1, true);

                smtp::work_item wi(config_->general.smtp_server);
                wi.set_sender(sender_);
                wi.set_recipients(ctx.good_recipients);
                wi.set_message(headers, ctx.encrypted_body);
                smtp_work_items.push_back(wi);
            }
        }

        if (!smtp_work_items.empty()) {
            smtp::client_multi cm(config_->general.smtp_server_timeout);

            for (const auto &wi: smtp_work_items)
                cm.add(wi);

            try {
                // XXX
                // If several messages are sent at once, it's possible only
                // some of them are going to be successfully sent, hence
                // creating a delicate situation. Returning SMFIS_TEMPFAIL
                // looks like the better choice, although when the message
                // arrives in milter again, it will end up being sent to the
                // same recipients once more.
                int failed_count = cm.perform();
                if (failed_count != 0) {
                    spdlog::warn("{}: {} out of {} emails failed during delivery, email is rejected temporarily",
                                 message_id_, failed_count, smtp_work_items.size());
                    return SMFIS_TEMPFAIL;
                }
            } catch (const std::runtime_error &e) {
                return SMFIS_TEMPFAIL;
            }
        }
    } catch (const boost::exception &e) {
        spdlog::error("{}: boost exception caught: {}", message_id_, boost::diagnostic_information(e));
        utils::dump_email dmp("dump", "exception-", connection_id_, message_id_, headers_, body_, false,
                              config_->general.dump_email_on_panic);
        return SMFIS_TEMPFAIL;
    } catch (const std::exception &e) {
        spdlog::error("{}: exception caught: {}", message_id_, e.what());
        utils::dump_email dmp("dump", "exception-", connection_id_, message_id_, headers_, body_, false,
                              config_->general.dump_email_on_panic);
        return SMFIS_TEMPFAIL;
    } catch (...) {
        utils::dump_email dmp("dump", "exception-", connection_id_, message_id_, headers_, body_, false,
                              config_->general.dump_email_on_panic);
        spdlog::debug("{}: unknown exception caught", message_id_);
        return SMFIS_TEMPFAIL;
    }

    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_abort()
{
    spdlog::debug("{}: aborted", message_id_);
    return SMFIS_CONTINUE;
}


void milter_message::replace_headers(const headers_type &headers)
{
    for (const auto &h: headers) {
        if (!h.modified)
            continue;

        if (smfi_chgheader(smfictx_, const_cast<char *>(h.name.c_str()), h.index,
                           const_cast<char *>(h.value.c_str())) == MI_FAILURE)
        {
            throw milter_exception("Failed to update/remove header " + h.name);
        }

        if (h.value.empty())
            spdlog::debug("{}: removed header {}", message_id_, h.name);
        else
            spdlog::debug("{}: updated header {}: {}", message_id_, h.name, h.value);
    }

    // Strip headers per configuration
    for (const auto &header: config_->general.strip_headers) {
        if (std::find_if(headers.begin(), headers.end(),
                         [&](const header_item &h) { return boost::iequals(h.name, header); }) == headers.end())
            continue;
        if (smfi_chgheader(smfictx_, const_cast<char *>(header.c_str()), 1, nullptr) == MI_FAILURE)
            throw milter_exception("failed to remove header " + header);
        spdlog::debug("{}: removed header \"{}\"", message_id_, header);
    }
}


bool milter_message::verify_signature()
{
    using namespace egpgcrypt;

    crypto c(GPGME_PROTOCOL_OpenPGP);
    memory_data_buffer body(body_);
    body.seek(0, data_buffer::SET);

    memory_data_buffer signature;
    signature.write("-----BEGIN PGP SIGNATURE-----\n\n");
    signature.write(signature_header_);
    signature.write("\n-----END PGP SIGNATURE-----");
    signature.seek(0, data_buffer::SET);

    if (c.verify(signature, body)) {
        spdlog::debug("{}: signature header verifies, removing {} header", message_id_, x_gwmilter_signature);

        if (smfi_chgheader(smfictx_, const_cast<char *>(x_gwmilter_signature.c_str()), 1, nullptr) == MI_FAILURE)
            throw milter_exception("failed to remove header " + x_gwmilter_signature);

        return true;
    }

    return false;
}


void milter_message::sign(const std::set<std::string> &keys, const std::string &in, std::string &out)
{
    using namespace egpgcrypt;

    spdlog::debug("{}: signing message size={}", message_id_, in.size());

    // always use PGP to sign
    crypto c(GPGME_PROTOCOL_OpenPGP);
    memory_data_buffer in_buf(in);
    memory_data_buffer out_buf;
    c.sign(keys, in_buf, out_buf);
    out = out_buf.content();

    auto pos = out.find("\n\n");
    if (pos == std::string::npos)
        throw std::runtime_error("invalid PGP signature: empty line not found");
    out.erase(0, pos + 2);

    pos = out.find("-----END PGP SIGNATURE-----");
    if (pos == std::string::npos)
        throw std::runtime_error("invalid PGP signature: end header not found");
    out.erase(pos);

    out.erase(std::remove_if(out.begin(), out.end(), [](int ch) { return ch == '\n'; }), out.end());
}


void milter_message::pack_header_value(std::string &value, std::string::size_type header_name_size,
                                       std::string::size_type max_line_size)
{
    // Limit the size to max_line_size per line. First line takes
    // into account the header name length plus two chars for ": "
    header_name_size += 2;

    std::string::size_type pos = 0;
    std::string::size_type line_len = max_line_size - header_name_size;
    while (pos + line_len < value.size()) {
        pos += std::min(line_len, value.size());
        value.insert(pos, "\r\n\t");
        // skip "\r\n", as it doesn't count towards the line length
        pos += 2;
        // compute the next line_len
        line_len = std::min(value.size() - pos, max_line_size);
    }
}


void milter_message::unpack_header_value(std::string &value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](int ch) { return ch == '\n' || ch == '\t'; }),
                value.end());
}


milter_message::email_context &milter_message::get_context(const cfg2::BaseEncryptionSection *section)
{
    auto it = std::find_if(contexts_.begin(), contexts_.end(),
                           [&section](const auto &pair) { return pair.first == section->sectionName; });

    if (it == contexts_.end()) {
        // there's no context for the current section,
        // therefore one needs to be created
        switch (section->encryption_protocol) {
        case cfg2::EncryptionProtocol::Pgp:
            return contexts_
                .emplace_back(section->sectionName, email_context{.body_handler = std::make_shared<pgp_body_handler>()})
                .second;
        case cfg2::EncryptionProtocol::Smime:
            return contexts_
                .emplace_back(section->sectionName,
                              email_context{.body_handler = std::make_shared<smime_body_handler>()})
                .second;
        case cfg2::EncryptionProtocol::Pdf: {
            // Safeguard: encryption_protocol guarantees the type, but verify at runtime
            const auto *pdf_section = dynamic_cast<const cfg2::PdfEncryptionSection *>(section);
            if (pdf_section == nullptr)
                throw std::runtime_error("PDF section type mismatch for: " + section->sectionName);
            return contexts_
                .emplace_back(section->sectionName,
                              email_context{.body_handler = std::make_shared<pdf_body_handler>(*pdf_section)})
                .second;
        }
        case cfg2::EncryptionProtocol::None:
            return contexts_
                .emplace_back(section->sectionName,
                              email_context{.body_handler = std::make_shared<noop_body_handler>()})
                .second;
        }
        throw std::runtime_error("Unknown encryption protocol: " +
                                 std::string(cfg2::toString(section->encryption_protocol)));
    }

    return it->second;
}


void milter_message::update_milter_recipients(const std::set<std::string> &good_recipients) const
{
    for (const auto &r: recipients_all_) {
        if (good_recipients.find(r) == good_recipients.end()) {
            smfi_delrcpt(smfictx_, const_cast<char *>(r.c_str()));
            spdlog::debug("{}: removed recipient {} from milter", message_id_, r);
        }
    }
}


void milter_message::set_reply(const char *p1, const char *p2, const char *p3) const
{
    smfi_setreply(smfictx_, const_cast<char *>(p1), const_cast<char *>(p2), const_cast<char *>(p3));
}


} // namespace gwmilter
