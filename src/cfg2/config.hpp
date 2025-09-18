#pragma once

#include "section_registry.hpp"
#include <fmt/core.h>
#include <regex>
#include <type_traits>
#include <vector>

namespace cfg2 {

// Static "general" section
struct GeneralSection : BaseSection {
    std::string milter_socket;
    bool daemonize = false;
    std::string user;
    std::string group;
    std::string log_type = "console";
    std::string log_facility = "mail";
    std::string log_priority = "info";
    int milter_timeout = -1;
    std::string smtp_server = "smtp://127.0.0.1";
    int smtp_server_timeout = -1;
    bool dump_email_on_panic = false;
    std::string signing_key;
    std::vector<std::string> strip_headers;

    void validate() const
    {
        if (milter_socket.empty())
            throw std::invalid_argument("Section 'general' must define milter_socket");

        if (log_type != "console" && log_type != "syslog")
            throw std::invalid_argument("Section 'general' must set log_type to 'console' or 'syslog'");

        if (milter_timeout < -1)
            throw std::invalid_argument("Section 'general' must set milter_timeout >= -1");

        if (smtp_server_timeout < -1)
            throw std::invalid_argument("Section 'general' must set smtp_server_timeout >= -1");

        if (!smtp_server.empty()) {
            static const std::regex smtp_pattern(R"(^smtps?://.+)");
            if (!std::regex_match(smtp_server, smtp_pattern))
                throw std::invalid_argument(
                    "Section 'general' must set smtp_server starting with 'smtp://' or 'smtps://' and include a host");
        }
    }
};

// Register GeneralSection as mandatory
REGISTER_STATIC_SECTION_MANDATORY(GeneralSection, "general", field("milter_socket", &GeneralSection::milter_socket),
                                  field("daemonize", &GeneralSection::daemonize), field("user", &GeneralSection::user),
                                  field("group", &GeneralSection::group), field("log_type", &GeneralSection::log_type),
                                  field("log_facility", &GeneralSection::log_facility),
                                  field("log_priority", &GeneralSection::log_priority),
                                  field("milter_timeout", &GeneralSection::milter_timeout),
                                  field("smtp_server", &GeneralSection::smtp_server),
                                  field("smtp_server_timeout", &GeneralSection::smtp_server_timeout),
                                  field("dump_email_on_panic", &GeneralSection::dump_email_on_panic),
                                  field("signing_key", &GeneralSection::signing_key),
                                  field("strip_headers", &GeneralSection::strip_headers))

// Encryption section types
struct BaseEncryptionSection : BaseDynamicSection {
    std::string encryption_protocol;
};

struct PgpEncryptionSection : BaseEncryptionSection {
    std::string key_not_found_policy;

    void validate() const
    {
        const auto &section = sectionName.empty() ? type : sectionName;

        if (encryption_protocol != "pgp")
            throw std::invalid_argument(fmt::format("Section '{}' must have encryption_protocol='pgp'", section));

        if (match.empty())
            throw std::invalid_argument(fmt::format("Section '{}' must have at least one match pattern", section));

        if (key_not_found_policy.empty())
            throw std::invalid_argument(fmt::format("Section '{}' must define key_not_found_policy", section));

        if (key_not_found_policy != "discard" && key_not_found_policy != "reject" && key_not_found_policy != "retrieve")
            throw std::invalid_argument(fmt::format(
                "Section '{}' must set key_not_found_policy to 'discard', 'reject', or 'retrieve'", section));
    }
};

REGISTER_DYNAMIC_SECTION_INLINE(PgpEncryptionSection, "pgp", field("match", &PgpEncryptionSection::match),
                                field("encryption_protocol", &PgpEncryptionSection::encryption_protocol),
                                field("key_not_found_policy", &PgpEncryptionSection::key_not_found_policy))

struct SmimeEncryptionSection : BaseEncryptionSection {
    std::string key_not_found_policy;

    void validate() const
    {
        const auto &section = sectionName.empty() ? type : sectionName;

        if (encryption_protocol != "smime")
            throw std::invalid_argument(fmt::format("Section '{}' must have encryption_protocol='smime'", section));

        if (match.empty())
            throw std::invalid_argument(fmt::format("Section '{}' must have at least one match pattern", section));

        if (key_not_found_policy.empty())
            throw std::invalid_argument(fmt::format("Section '{}' must define key_not_found_policy", section));

        if (key_not_found_policy != "discard" && key_not_found_policy != "reject")
            throw std::invalid_argument(fmt::format("Section '{}' must set key_not_found_policy to 'discard' or "
                                                    "'reject' (retrieve is not supported for S/MIME)",
                                                    section));
    }
};

REGISTER_DYNAMIC_SECTION_INLINE(SmimeEncryptionSection, "smime", field("match", &SmimeEncryptionSection::match),
                                field("encryption_protocol", &SmimeEncryptionSection::encryption_protocol),
                                field("key_not_found_policy", &SmimeEncryptionSection::key_not_found_policy))

struct PdfEncryptionSection : BaseEncryptionSection {
    std::string email_body_replacement;
    std::string pdf_main_page_if_missing;
    std::string pdf_attachment = "email.pdf";
    std::string pdf_password;
    std::string pdf_font_path;
    float pdf_font_size = 10.0f;
    float pdf_margin = 10.0f;

    void validate() const
    {
        const auto &section = sectionName.empty() ? type : sectionName;

        if (encryption_protocol != "pdf")
            throw std::invalid_argument(fmt::format("Section '{}' must have encryption_protocol='pdf'", section));

        if (match.empty())
            throw std::invalid_argument(fmt::format("Section '{}' must have at least one match pattern", section));

        if (pdf_font_size <= 0.0f)
            throw std::invalid_argument(
                fmt::format("Section '{}' must set pdf_font_size to a positive value", section));

        if (pdf_margin < 0.0f)
            throw std::invalid_argument(
                fmt::format("Section '{}' must set pdf_margin to a non-negative value", section));

        if (pdf_attachment.empty())
            throw std::invalid_argument(fmt::format("Section '{}' must define pdf_attachment", section));
    }
};

REGISTER_DYNAMIC_SECTION_INLINE(PdfEncryptionSection, "pdf", field("match", &PdfEncryptionSection::match),
                                field("encryption_protocol", &PdfEncryptionSection::encryption_protocol),
                                field("email_body_replacement", &PdfEncryptionSection::email_body_replacement),
                                field("pdf_main_page_if_missing", &PdfEncryptionSection::pdf_main_page_if_missing),
                                field("pdf_attachment", &PdfEncryptionSection::pdf_attachment),
                                field("pdf_password", &PdfEncryptionSection::pdf_password),
                                field("pdf_font_path", &PdfEncryptionSection::pdf_font_path),
                                field("pdf_font_size", &PdfEncryptionSection::pdf_font_size),
                                field("pdf_margin", &PdfEncryptionSection::pdf_margin))

struct NoneEncryptionSection : BaseEncryptionSection {
    void validate() const
    {
        const auto &section = sectionName.empty() ? type : sectionName;

        if (encryption_protocol != "none")
            throw std::invalid_argument(fmt::format("Section '{}' must have encryption_protocol='none'", section));

        if (match.empty())
            throw std::invalid_argument(fmt::format("Section '{}' must have at least one match pattern", section));
    }
};

REGISTER_DYNAMIC_SECTION_INLINE(NoneEncryptionSection, "none", field("match", &NoneEncryptionSection::match),
                                field("encryption_protocol", &NoneEncryptionSection::encryption_protocol))

// Main configuration
struct Config {
    GeneralSection general;
    std::vector<std::unique_ptr<BaseEncryptionSection>> encryptionSections;

    // Find first encryption section that matches the given recipient
    // Returns raw pointer safe as observer - lifetime tied to parent Config shared_ptr
    const BaseEncryptionSection *find_match(const std::string &rcpt) const
    {
        for (const auto &section: encryptionSections)
            if (section->matches(rcpt))
                return section.get();
        return nullptr;
    }

    // Cross-section validation - validates relationships between sections
    void validate() const
    {
        // When multiple encryption sections are present, signing_key and smtp_server are required
        if (encryptionSections.size() > 1) {
            if (general.signing_key.empty())
                throw std::invalid_argument("signing_key is required when multiple encryption sections are present");

            if (general.smtp_server.empty())
                throw std::invalid_argument("smtp_server is required when multiple encryption sections are present");
        }
    }
};

// Custom deserializer for Config to handle mixed static/dynamic sections
template<> struct is_deserializable_struct<Config> : std::true_type { };

template<> Config deserialize<Config>(const ConfigNode &node);

} // namespace cfg2
