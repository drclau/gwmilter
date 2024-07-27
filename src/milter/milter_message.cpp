#include "milter_message.hpp"
#include "cfg/cfg.hpp"
#include "handlers/body_handler.hpp"
#include "logger/logger.hpp"
#include "milter_exception.hpp"
#include "smtp/smtp_client.hpp"
#include "utils/dump_email.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <libmilter/mfapi.h>
#include <memory>
#include <string>
#include <vector>

namespace gwmilter {

const std::string milter_message::x_gwmilter_signature = "X-GWMilter-Signature";

milter_message::milter_message(SMFICTX *ctx, const std::string &connection_id)
    : smfictx_{ctx}, connection_id_{connection_id}, message_id_{uid_gen_.generate()}
{
    L_INFO << message_id_ << ": begin message (connection_id=" << connection_id_ << ")";
}


milter_message::~milter_message()
{
    L_INFO << message_id_ << ": end message (connection_id=" << connection_id_ << ")";
}


sfsistat milter_message::on_envfrom(const std::vector<std::string> &args)
{
    if (args.empty()) {
        L_WARN << message_id_ << ": sender is empty";
        return SMFIS_CONTINUE;
    }

    L_INFO << message_id_ << ": from=" << args[0];
    sender_ = args[0];
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_envrcpt(const std::vector<std::string> &args)
{
    if (args.empty()) {
        L_ERR << message_id_ << ": recipient is empty";
        return SMFIS_REJECT;
    }

    const std::string &rcpt = args[0];
    L_INFO << message_id_ << ": to=" << rcpt;
    std::shared_ptr<cfg::encryption_section_handler> gs = cfg::cfg::inst().find_match(rcpt);

    if (gs == nullptr) {
        // recipient doesn't match any section, hence reject it
        set_reply("550", "5.7.1", "recipient does not match any configuration section");
        L_WARN << message_id_ << ": recipient " << rcpt << " was not found in any section, rejecting";
        return SMFIS_REJECT;
    }

    L_DEBUG << message_id_ << ": recipient " << rcpt << " was found in section " << gs->name();
    email_context &context = get_context(gs);

    if (context.body_handler->has_public_key(rcpt)) {
        L_DEBUG << message_id_ << ": found public key in local keyring for " << rcpt;
        context.recipients[rcpt] = true;
    } else {
        L_DEBUG << message_id_ << ": couldn't find public key in local keyring for " << rcpt;
        switch (gs->get<cfg::key_not_found_policy_enum>("key_not_found_policy")) {
        case cfg::discard:
            L_WARN << message_id_ << ": discarding recipient " << rcpt;
            context.recipients[rcpt] = false;
            break;

        case cfg::retrieve:
            // XXX: maybe the key importing should be done in another place to
            // avoid delays or timeouts in this part of the MTA-to-MTA communication
            if (context.body_handler->import_public_key(rcpt)) {
                L_INFO << message_id_ << ": imported new public key for " << rcpt;
                context.recipients[rcpt] = true;
            } else {
                L_WARN << message_id_ << ": failed to import new public key for " << rcpt;
                context.recipients[rcpt] = false;
            }
            break;

        case cfg::reject:
            set_reply("550", "5.7.1", "Recipient does not have a public key");
            L_WARN << message_id_ << ": rejected recipient " << rcpt << " due to missing public key";
            return SMFIS_REJECT;
        }
    }

    recipients_all_.emplace_back(rcpt);
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_data()
{
    L_DEBUG << message_id_ << ": data";

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
        L_WARN << message_id_ << ": no recipient matches the existing configuration sections, rejecting email";
        return SMFIS_REJECT;
    }

    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_header(const std::string &headerf, const std::string &headerv)
{
    L_DEBUG << message_id_ << ": header " << headerf << "=" << headerv;

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
    L_DEBUG << message_id_ << ": end-of-headers";
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_body(const std::string &body)
{
    L_DEBUG << message_id_ << ": body size=" << body.size();
    body_ += body;
    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_eom()
{
    L_DEBUG << message_id_ << ": end-of-message";

    utils::dump_email dmp("dump", "crash-", connection_id_, message_id_, headers_, body_, true);

    try {
        if (!signature_header_.empty()) {
            if (verify_signature()) {
                L_INFO << message_id_ << ": signature header verifies, allowing email to pass";
                return SMFIS_CONTINUE;
            } else {
                L_ERR << message_id_ << ": rejecting email due to failure while verifying the signature";
                set_reply("550", nullptr, "failed to verify the signature");
                return SMFIS_REJECT;
            }
        }
    } catch (const std::exception &e) {
        // TODO: std::runtime_error should be replaced with something more specific
        L_ERR << message_id_ << ": rejecting email due to failure while verifying the signature: " << e.what();
        set_reply("550", nullptr, "failed to verify the signature");
        return SMFIS_REJECT;
    }

    try {
        // process all matching configuration sections for current milter message
        auto g = cfg::cfg::inst().section(cfg::GENERAL_SECTION);
        std::vector<smtp::work_item> smtp_work_items;

        bool milter_body_replaced = false;
        for (auto &[section, ctx]: contexts_) {
            L_DEBUG << message_id_ << ": processing section " << section;

            if (ctx.good_recipients.empty()) {
                L_DEBUG << message_id_ << ": section " << section << " has no recipients left";
                continue;
            }

            ctx.body_handler->write(body_);
            ctx.body_handler->encrypt(ctx.good_recipients, *ctx.encrypted_body);

            int i = 1;
            for (const auto &r: ctx.body_handler->failed_recipients()) {
                L_DEBUG << message_id_ << ": failed key #" << i << " = " << r;
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
                keys.insert(g->get<std::string>("signing_key"));
                std::string signature;
                sign(keys, *ctx.encrypted_body, signature);
                pack_header_value(signature, x_gwmilter_signature.size());

                headers.emplace_back(x_gwmilter_signature, signature, 1, true);

                smtp::work_item wi(g->get<std::string>("smtp_server"));
                wi.set_sender(sender_);
                wi.set_recipients(ctx.good_recipients);
                wi.set_message(headers, ctx.encrypted_body);
                smtp_work_items.push_back(wi);
            }
        }

        if (!smtp_work_items.empty()) {
            smtp::client_multi cm(g->get<unsigned int>("smtp_server_timeout"));

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
                    L_WARN << message_id_ << ": " << failed_count << " out of " << smtp_work_items.size()
                           << " emails failed during delivery, email is rejected temporarily";

                    return SMFIS_TEMPFAIL;
                }
            } catch (const std::runtime_error &e) {
                return SMFIS_TEMPFAIL;
            }
        }
    } catch (const boost::exception &e) {
        L_ERR << message_id_ << ": boost exception caught: " << boost::diagnostic_information(e);
        utils::dump_email dmp("dump", "exception-", connection_id_, message_id_, headers_, body_, false);
        return SMFIS_TEMPFAIL;
    } catch (const std::exception &e) {
        L_ERR << message_id_ << ": exception caught: " << e.what();
        utils::dump_email dmp("dump", "exception-", connection_id_, message_id_, headers_, body_, false);
        return SMFIS_TEMPFAIL;
    } catch (...) {
        utils::dump_email dmp("dump", "exception-", connection_id_, message_id_, headers_, body_, false);
        L_DEBUG << message_id_ << ": unknown exception caught";
        return SMFIS_TEMPFAIL;
    }

    return SMFIS_CONTINUE;
}


sfsistat milter_message::on_abort()
{
    L_DEBUG << message_id_ << ": aborted";
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
            L_DEBUG << message_id_ << ": removed header " << h.name;
        else
            L_DEBUG << message_id_ << ": updated header " << h.name << ": " << h.value;
    }

    // Strip headers per configuration
    auto strip_headers = cfg::cfg::inst().section(cfg::GENERAL_SECTION)->get<std::vector<std::string>>("strip_headers");
    for (const auto &header: strip_headers) {
        if (std::find_if(headers.begin(), headers.end(),
                         [&](const header_item &h) { return boost::iequals(h.name, header); }) == headers.end())
            continue;
        if (smfi_chgheader(smfictx_, const_cast<char *>(header.c_str()), 1, nullptr) == MI_FAILURE)
            throw milter_exception("failed to remove header " + header);
        L_DEBUG << message_id_ << ": removed header \"" << header << "\"";
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
        L_DEBUG << message_id_ << ": signature header verifies, removing " << x_gwmilter_signature << " header";

        if (smfi_chgheader(smfictx_, const_cast<char *>(x_gwmilter_signature.c_str()), 1, nullptr) == MI_FAILURE)
            throw milter_exception("failed to remove header " + x_gwmilter_signature);

        return true;
    }

    return false;
}


void milter_message::sign(const std::set<std::string> &keys, const std::string &in, std::string &out)
{
    using namespace egpgcrypt;

    L_DEBUG << message_id_ << ": signing message size=" << in.size();

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


milter_message::email_context &milter_message::get_context(const std::shared_ptr<cfg::encryption_section_handler> &gs)
{
    auto it =
        std::find_if(contexts_.begin(), contexts_.end(), [&gs](const auto &pair) { return pair.first == gs->name(); });

    if (it == contexts_.end()) {
        // there's no context for the current section,
        // therefore one needs to be created
        auto protocol = gs->get<cfg::encryption_protocol_enum>("encryption_protocol");

        switch (protocol) {
        case cfg::pgp:
            return contexts_
                .emplace_back(gs->name(), email_context{.body_handler = std::make_shared<pgp_body_handler>()})
                .second;
        case cfg::smime:
            return contexts_
                .emplace_back(gs->name(), email_context{.body_handler = std::make_shared<smime_body_handler>()})
                .second;
        case cfg::pdf:
            return contexts_
                .emplace_back(gs->name(), email_context{.body_handler = std::make_shared<pdf_body_handler>(gs)})
                .second;
        case cfg::none:
            return contexts_
                .emplace_back(gs->name(), email_context{.body_handler = std::make_shared<noop_body_handler>(gs)})
                .second;
        }
    }

    return it->second;
}


void milter_message::update_milter_recipients(const std::set<std::string> &good_recipients) const
{
    for (const auto &r: recipients_all_) {
        if (good_recipients.find(r) == good_recipients.end()) {
            smfi_delrcpt(smfictx_, const_cast<char *>(r.c_str()));
            L_DEBUG << message_id_ << ": removed recipient " << r << " from milter";
        }
    }
}


void milter_message::set_reply(const char *p1, const char *p2, const char *p3) const
{
    smfi_setreply(smfictx_, const_cast<char *>(p1), const_cast<char *>(p2), const_cast<char *>(p3));
}


} // namespace gwmilter