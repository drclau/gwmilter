#pragma once

#include "dynamic_section.hpp"
#include <vector>

namespace cfg2 {

// ============= INI CONFIGURATION EXAMPLE =============

// Static general section - always present
struct GeneralSection {
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
    std::string strip_headers;
};

// Encryption section types
struct BaseEncryptionSection : BaseDynamicSection {
    std::string encryption_protocol;
};

struct PgpEncryptionSection : BaseEncryptionSection {
    std::string key_not_found_policy;
};

struct SmimeEncryptionSection : BaseEncryptionSection {
    std::string key_not_found_policy;
};

struct PdfEncryptionSection : BaseEncryptionSection {
    std::string email_body_replacement;
    std::string pdf_main_page_if_missing;
    std::string pdf_attachment = "email.pdf";
    std::string pdf_password;
    std::string pdf_font_path;
    float pdf_font_size = 10.0f;
    float pdf_margin = 10.0f;
};

struct NoneEncryptionSection : BaseEncryptionSection {
};

// Main INI configuration
struct IniConfig {
    GeneralSection general;
    std::vector<std::unique_ptr<BaseEncryptionSection>> encryptionSections;
    
    // Find first encryption section that matches the given value
    BaseEncryptionSection* find_match(const std::string& value) const {
        for (const auto& section : encryptionSections) {
            if (section->matches(value)) {
                return section.get();
            }
        }
        return nullptr;
    }
};

// Custom deserializer for IniConfig to handle mixed static/dynamic sections
template<>
struct is_deserializable_struct<IniConfig> : std::true_type {};

template<>
IniConfig deserialize<IniConfig>(const TreeNode& node);

} // namespace cfg2