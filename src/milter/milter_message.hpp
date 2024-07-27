#pragma once
#include "cfg/cfg.hpp"
#include "handlers/body_handler.hpp"
#include "logger/logger.hpp"
#include "smtp/smtp_client.hpp"
#include "utils/uid_generator.hpp"
#include <crypto.hpp>
#include <data_buffers.hpp>
#include <libmilter/mfapi.h>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gwmilter {

class milter_message {
    // forward declaration
    struct email_context;

public:
    explicit milter_message(SMFICTX *ctx, const std::string &connection_id);
    ~milter_message();
    milter_message(const milter_message &) = delete;
    milter_message &operator=(const milter_message &) = delete;

    sfsistat on_envfrom(const std::vector<std::string> &args);
    sfsistat on_envrcpt(const std::vector<std::string> &args);
    sfsistat on_data();
    sfsistat on_header(const std::string &headerf, const std::string &headerv);
    sfsistat on_eoh();
    sfsistat on_body(const std::string &body);
    sfsistat on_eom();
    sfsistat on_abort();

private:
    void replace_headers(const headers_type &headers);
    bool verify_signature();
    void sign(const std::set<std::string> &keys, const std::string &in, std::string &out);
    static void pack_header_value(std::string &value, std::string::size_type header_name_size,
                                  std::string::size_type max_line_size = RFC5322_MAX_LINE_SIZE);
    static void unpack_header_value(std::string &value);
    // Returns the context attached to the configuration section; creates a new one if necessary
    email_context &get_context(const std::shared_ptr<cfg::encryption_section_handler> &gs);
    // Removes milter recipients that are not present in good_recipients
    void update_milter_recipients(const std::set<std::string> &good_recipients) const;
    void set_reply(const char *, const char *, const char *) const;

private:
    const static std::string x_gwmilter_signature;

    SMFICTX *smfictx_;

    uid_generator uid_gen_;
    std::string connection_id_;
    std::string message_id_;

    std::string sender_;
    std::string body_;
    // stores all recipients, to make it easy to remove unwanted recipients from milter
    std::vector<std::string> recipients_all_;
    std::string signature_header_;
    // XXX: currently only used for debugging
    std::string headers_;

    // holds email details, used to store data per configuration section
    struct email_context {
        // bool marks the presence or absence of associated public key
        std::map<std::string, bool> recipients;
        // keeps only the recipients for which public keys were found
        std::set<std::string> good_recipients;
        std::shared_ptr<body_handler_base> body_handler;

        // libmilter does not make a copy of the buffer when `smfi_replacebody()` is called.
        // Hence, we need to keep the buffer alive until the end of the message.
        std::shared_ptr<std::string> encrypted_body = std::make_shared<std::string>();
    };

    // order matters, hence using std::vector
    using contexts_type = std::vector<std::pair<std::string, email_context>>;
    contexts_type contexts_;

    static const int RFC5322_MAX_LINE_SIZE = 78;
};

} // end namespace gwmilter
