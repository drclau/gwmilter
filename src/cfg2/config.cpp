#include "config.hpp"
#include <regex>
#include <stdexcept>

namespace cfg2 {

// --------------------------------------------------------------------------------
// Forward declare validation specializations before registrations

// Enable validation for all section types
template<> struct has_validation<GeneralSection> : std::true_type { };

template<> struct has_validation<PgpEncryptionSection> : std::true_type { };

template<> struct has_validation<SmimeEncryptionSection> : std::true_type { };

template<> struct has_validation<PdfEncryptionSection> : std::true_type { };

template<> struct has_validation<NoneEncryptionSection> : std::true_type { };

// --------------------------------------------------------------------------------
// Validation function forward declarations

template<> void validate<GeneralSection>(const GeneralSection &section);

template<> void validate<PgpEncryptionSection>(const PgpEncryptionSection &section);

template<> void validate<SmimeEncryptionSection>(const SmimeEncryptionSection &section);

template<> void validate<PdfEncryptionSection>(const PdfEncryptionSection &section);

template<> void validate<NoneEncryptionSection>(const NoneEncryptionSection &section);

// Register static section
REGISTER_STATIC_SECTION(GeneralSection, "general", field("milter_socket", &GeneralSection::milter_socket),
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

// Register encryption section types
REGISTER_DYNAMIC_SECTION(PgpEncryptionSection, "pgp", field("match", &PgpEncryptionSection::match),
                         field("encryption_protocol", &PgpEncryptionSection::encryption_protocol),
                         field("key_not_found_policy", &PgpEncryptionSection::key_not_found_policy))

REGISTER_DYNAMIC_SECTION(SmimeEncryptionSection, "smime", field("match", &SmimeEncryptionSection::match),
                         field("encryption_protocol", &SmimeEncryptionSection::encryption_protocol),
                         field("key_not_found_policy", &SmimeEncryptionSection::key_not_found_policy))

REGISTER_DYNAMIC_SECTION(PdfEncryptionSection, "pdf", field("match", &PdfEncryptionSection::match),
                         field("encryption_protocol", &PdfEncryptionSection::encryption_protocol),
                         field("email_body_replacement", &PdfEncryptionSection::email_body_replacement),
                         field("pdf_main_page_if_missing", &PdfEncryptionSection::pdf_main_page_if_missing),
                         field("pdf_attachment", &PdfEncryptionSection::pdf_attachment),
                         field("pdf_password", &PdfEncryptionSection::pdf_password),
                         field("pdf_font_path", &PdfEncryptionSection::pdf_font_path),
                         field("pdf_font_size", &PdfEncryptionSection::pdf_font_size),
                         field("pdf_margin", &PdfEncryptionSection::pdf_margin))

REGISTER_DYNAMIC_SECTION(NoneEncryptionSection, "none", field("match", &NoneEncryptionSection::match),
                         field("encryption_protocol", &NoneEncryptionSection::encryption_protocol))

// Implementation of custom deserializer for Config
template<> Config deserialize<Config>(const ConfigNode &node)
{
    Config config{};

    // Process all sections in order using the unified registry
    for (const auto &child: node.children) {
        // Handle static sections by name
        if (SectionRegistry::hasSection(child.key)) {
            auto section = SectionRegistry::create(child.key, child);

            if (auto *generalSection = dynamic_cast<GeneralSection *>(section.get()))
                config.general = *generalSection;
        }
        // Handle dynamic sections by their encryption_protocol
        else
        {
            const ConfigNode *protocolNode = child.findChild("encryption_protocol");
            if (protocolNode && DynamicSectionRegistry::hasType(protocolNode->value)) {
                auto section = DynamicSectionRegistry::create(protocolNode->value, child);
                config.encryptionSections.push_back(
                    std::unique_ptr<BaseEncryptionSection>(static_cast<BaseEncryptionSection *>(section.release())));
            }
        }
    }

    return config;
}

// --------------------------------------------------------------------------------
// Validation implementations

template<> void validate<GeneralSection>(const GeneralSection &section)
{
    // Validate milter_socket is not empty
    if (section.milter_socket.empty())
        throw std::invalid_argument("milter_socket cannot be empty");

    // Validate log_type is valid
    if (section.log_type != "console" && section.log_type != "syslog")
        throw std::invalid_argument("log_type must be 'console' or 'syslog'");

    // Validate timeout values
    if (section.milter_timeout < -1)
        throw std::invalid_argument("milter_timeout must be >= -1 (-1 means infinite)");

    if (section.smtp_server_timeout < -1)
        throw std::invalid_argument("smtp_server_timeout must be >= -1 (-1 means infinite)");

    // Validate smtp_server is not empty
    if (section.smtp_server.empty())
        throw std::invalid_argument("smtp_server cannot be empty");

    // Basic URL format validation for smtp_server
    if (section.smtp_server.find("://") == std::string::npos)
        throw std::invalid_argument("smtp_server must be a valid URL (e.g., smtp://host:port)");
}

template<> void validate<PgpEncryptionSection>(const PgpEncryptionSection &section)
{
    // Validate encryption_protocol
    if (section.encryption_protocol != "pgp")
        throw std::invalid_argument("PgpEncryptionSection must have encryption_protocol='pgp'");

    // Validate key_not_found_policy
    if (!section.key_not_found_policy.empty() && section.key_not_found_policy != "discard" &&
        section.key_not_found_policy != "reject" && section.key_not_found_policy != "retrieve")
    {
        throw std::invalid_argument("key_not_found_policy must be 'discard', 'reject', or 'retrieve'");
    }

    // Validate match patterns are not empty
    if (section.match.empty())
        throw std::invalid_argument("PgpEncryptionSection must have at least one match pattern");
}

template<> void validate<SmimeEncryptionSection>(const SmimeEncryptionSection &section)
{
    // Validate encryption_protocol
    if (section.encryption_protocol != "smime")
        throw std::invalid_argument("SmimeEncryptionSection must have encryption_protocol='smime'");

    // Validate key_not_found_policy (retrieve is not allowed for S/MIME)
    if (!section.key_not_found_policy.empty() && section.key_not_found_policy != "discard" &&
        section.key_not_found_policy != "reject")
    {
        throw std::invalid_argument(
            "key_not_found_policy must be 'discard' or 'reject' (retrieve is not supported for S/MIME)");
    }

    // Validate match patterns are not empty
    if (section.match.empty())
        throw std::invalid_argument("SmimeEncryptionSection must have at least one match pattern");
}

template<> void validate<PdfEncryptionSection>(const PdfEncryptionSection &section)
{
    // Validate encryption_protocol
    if (section.encryption_protocol != "pdf")
        throw std::invalid_argument("PdfEncryptionSection must have encryption_protocol='pdf'");

    // Validate positive float values
    if (section.pdf_font_size <= 0.0f)
        throw std::invalid_argument("pdf_font_size must be positive");

    if (section.pdf_margin < 0.0f)
        throw std::invalid_argument("pdf_margin must be non-negative");

    // Validate match patterns are not empty
    if (section.match.empty())
        throw std::invalid_argument("PdfEncryptionSection must have at least one match pattern");

    // Validate pdf_attachment is not empty
    if (section.pdf_attachment.empty())
        throw std::invalid_argument("pdf_attachment cannot be empty");
}

template<> void validate<NoneEncryptionSection>(const NoneEncryptionSection &section)
{
    // Validate encryption_protocol
    if (section.encryption_protocol != "none")
        throw std::invalid_argument("NoneEncryptionSection must have encryption_protocol='none'");

    // Validate match patterns are not empty
    if (section.match.empty())
        throw std::invalid_argument("NoneEncryptionSection must have at least one match pattern");
}

} // namespace cfg2