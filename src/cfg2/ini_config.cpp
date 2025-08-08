#include "ini_config.hpp"

namespace cfg2 {

// Register static section
REGISTER_STRUCT(GeneralSection,
    field("milter_socket", &GeneralSection::milter_socket),
    field("daemonize", &GeneralSection::daemonize),
    field("user", &GeneralSection::user),
    field("group", &GeneralSection::group),
    field("log_type", &GeneralSection::log_type),
    field("log_facility", &GeneralSection::log_facility),
    field("log_priority", &GeneralSection::log_priority),
    field("milter_timeout", &GeneralSection::milter_timeout),
    field("smtp_server", &GeneralSection::smtp_server),
    field("smtp_server_timeout", &GeneralSection::smtp_server_timeout),
    field("dump_email_on_panic", &GeneralSection::dump_email_on_panic),
    field("signing_key", &GeneralSection::signing_key),
    field("strip_headers", &GeneralSection::strip_headers)
)

// Register encryption section types
REGISTER_DYNAMIC_SECTION(PgpEncryptionSection, "pgp",
    field("match", &PgpEncryptionSection::match),
    field("encryption_protocol", &PgpEncryptionSection::encryption_protocol),
    field("key_not_found_policy", &PgpEncryptionSection::key_not_found_policy)
)

REGISTER_DYNAMIC_SECTION(SmimeEncryptionSection, "smime",
    field("match", &SmimeEncryptionSection::match),
    field("encryption_protocol", &SmimeEncryptionSection::encryption_protocol),
    field("key_not_found_policy", &SmimeEncryptionSection::key_not_found_policy)
)

REGISTER_DYNAMIC_SECTION(PdfEncryptionSection, "pdf",
    field("match", &PdfEncryptionSection::match),
    field("encryption_protocol", &PdfEncryptionSection::encryption_protocol),
    field("email_body_replacement", &PdfEncryptionSection::email_body_replacement),
    field("pdf_main_page_if_missing", &PdfEncryptionSection::pdf_main_page_if_missing),
    field("pdf_attachment", &PdfEncryptionSection::pdf_attachment),
    field("pdf_password", &PdfEncryptionSection::pdf_password),
    field("pdf_font_path", &PdfEncryptionSection::pdf_font_path),
    field("pdf_font_size", &PdfEncryptionSection::pdf_font_size),
    field("pdf_margin", &PdfEncryptionSection::pdf_margin)
)

REGISTER_DYNAMIC_SECTION(NoneEncryptionSection, "none",
    field("match", &NoneEncryptionSection::match),
    field("encryption_protocol", &NoneEncryptionSection::encryption_protocol)
)

// Implementation of custom deserializer for IniConfig
template<>
IniConfig deserialize<IniConfig>(const TreeNode& node) {
    IniConfig config{};
    
    // Deserialize static general section
    const TreeNode* generalNode = node.findChild("general");
    if (generalNode) {
        config.general = deserialize<GeneralSection>(*generalNode);
    }
    
    // Deserialize encryption sections in order
    for (const auto& child : node.children) {
        if (child.key != "general") {
            const TreeNode* protocolNode = child.findChild("encryption_protocol");
            if (protocolNode && DynamicSectionRegistry::hasType(protocolNode->value)) {
                auto section = DynamicSectionRegistry::create(protocolNode->value, child);
                config.encryptionSections.push_back(
                    std::unique_ptr<BaseEncryptionSection>(
                        static_cast<BaseEncryptionSection*>(section.release())
                    )
                );
            }
        }
    }
    
    return config;
}

} // namespace cfg2