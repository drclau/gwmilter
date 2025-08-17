#pragma once

#include "section_registry.hpp"
#include <vector>

namespace cfg2 {

// ============= INI CONFIGURATION EXAMPLE =============

// Static general section - always present
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
            throw std::invalid_argument("milter_socket cannot be empty");

        if (log_type != "console" && log_type != "syslog")
            throw std::invalid_argument("log_type must be 'console' or 'syslog'");

        if (milter_timeout < -1)
            throw std::invalid_argument("milter_timeout must be >= -1 (-1 means infinite)");

        if (smtp_server_timeout < -1)
            throw std::invalid_argument("smtp_server_timeout must be >= -1 (-1 means infinite)");

        if (!smtp_server.empty() && smtp_server.find("://") == std::string::npos)
            throw std::invalid_argument("smtp_server must be a valid URL (e.g., smtp://host:port)");
    }
};

// Register GeneralSection next to its definition
REGISTER_STATIC_SECTION_INLINE(GeneralSection, "general", field("milter_socket", &GeneralSection::milter_socket),
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
        if (encryption_protocol != "pgp")
            throw std::invalid_argument("PgpEncryptionSection must have encryption_protocol='pgp'");

        if (!key_not_found_policy.empty() && key_not_found_policy != "discard" && key_not_found_policy != "reject" &&
            key_not_found_policy != "retrieve")
        {
            throw std::invalid_argument("key_not_found_policy must be 'discard', 'reject', or 'retrieve'");
        }

        if (match.empty())
            throw std::invalid_argument("PgpEncryptionSection must have at least one match pattern");
    }
};

REGISTER_DYNAMIC_SECTION_INLINE(PgpEncryptionSection, "pgp", field("match", &PgpEncryptionSection::match),
                                field("encryption_protocol", &PgpEncryptionSection::encryption_protocol),
                                field("key_not_found_policy", &PgpEncryptionSection::key_not_found_policy))

struct SmimeEncryptionSection : BaseEncryptionSection {
    std::string key_not_found_policy;

    void validate() const
    {
        if (encryption_protocol != "smime")
            throw std::invalid_argument("SmimeEncryptionSection must have encryption_protocol='smime'");

        if (!key_not_found_policy.empty() && key_not_found_policy != "discard" && key_not_found_policy != "reject") {
            throw std::invalid_argument(
                "key_not_found_policy must be 'discard' or 'reject' (retrieve is not supported for S/MIME)");
        }

        if (match.empty())
            throw std::invalid_argument("SmimeEncryptionSection must have at least one match pattern");
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
        if (encryption_protocol != "pdf")
            throw std::invalid_argument("PdfEncryptionSection must have encryption_protocol='pdf'");

        if (pdf_font_size <= 0.0f)
            throw std::invalid_argument("pdf_font_size must be positive");

        if (pdf_margin < 0.0f)
            throw std::invalid_argument("pdf_margin must be non-negative");

        if (match.empty())
            throw std::invalid_argument("PdfEncryptionSection must have at least one match pattern");

        if (pdf_attachment.empty())
            throw std::invalid_argument("pdf_attachment cannot be empty");
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
        if (encryption_protocol != "none")
            throw std::invalid_argument("NoneEncryptionSection must have encryption_protocol='none'");

        if (match.empty())
            throw std::invalid_argument("NoneEncryptionSection must have at least one match pattern");
    }
};

REGISTER_DYNAMIC_SECTION_INLINE(NoneEncryptionSection, "none", field("match", &NoneEncryptionSection::match),
                                field("encryption_protocol", &NoneEncryptionSection::encryption_protocol))

// Main INI configuration
struct Config {
    GeneralSection general;
    std::vector<std::unique_ptr<BaseEncryptionSection>> encryptionSections;

    // Find first encryption section that matches the given value
    // Returns raw pointer safe as observer - lifetime tied to parent Config shared_ptr
    const BaseEncryptionSection *find_match(const std::string &value) const
    {
        for (const auto &section: encryptionSections)
            if (section->matches(value))
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
