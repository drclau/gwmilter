#include "core.hpp"
#include <iostream>

using namespace cfg2;

int main() {
    // Example INI structure representing:
    // [general]
    // milter_socket=unix:/var/run/gwmilter/gwmilter.sock
    // daemonize=true
    // log_type=syslog
    // log_facility=mail
    // log_priority=info
    //
    // [encrypt_internal]
    // match=.*@company\.com,.*@internal\.org
    // encryption_protocol=pgp
    // key_not_found_policy=retrieve
    //
    // [encrypt_external_pdf]
    // match=.*@external\.com
    // encryption_protocol=pdf
    // email_body_replacement=/etc/gwmilter/pdf_body.txt
    // pdf_main_page_if_missing=/etc/gwmilter/pdf_cover.pdf
    // pdf_password=secret123
    //
    // [no_encryption]
    // match=.*@public\.org
    // encryption_protocol=none
    
    TreeNode root{"config","",{
        {"general","",{
            {"milter_socket","unix:/var/run/gwmilter/gwmilter.sock"},
            {"daemonize","true"},
            {"log_type","syslog"},
            {"log_facility","mail"},
            {"log_priority","info"},
            {"smtp_server","smtp://127.0.0.1"}
        }},
        {"encrypt_internal","",{
            {"encryption_protocol","pgp"},
            {"match",".*@company\\.com,.*@internal\\.org",{}},
            {"key_not_found_policy","retrieve"}
        }},
        {"encrypt_external_pdf","",{
            {"encryption_protocol","pdf"},
            {"match",".*@external\\.com",{}},
            {"email_body_replacement","/etc/gwmilter/pdf_body.txt"},
            {"pdf_main_page_if_missing","/etc/gwmilter/pdf_cover.pdf"},
            {"pdf_password","secret123"}
        }},
        {"no_encryption","",{
            {"encryption_protocol","none"},
            {"match",".*@public\\.org",{}}
        }}
    }};

    IniConfig config = parse<IniConfig>(root);

    std::cout << "=== General Configuration ===\n";
    std::cout << "Milter Socket: " << config.general.milter_socket << "\n";
    std::cout << "Daemonize: " << (config.general.daemonize ? "true" : "false") << "\n";
    std::cout << "Log Type: " << config.general.log_type << "\n";
    std::cout << "Log Facility: " << config.general.log_facility << "\n";
    std::cout << "Log Priority: " << config.general.log_priority << "\n";
    std::cout << "SMTP Server: " << config.general.smtp_server << "\n\n";

    std::cout << "=== Encryption Sections ===\n";
    for (const auto& section : config.encryptionSections) {
        std::cout << "[" << section->sectionName << "] (protocol: " << section->encryption_protocol << ")\n";
        
        if (auto* pgp = dynamic_cast<PgpEncryptionSection*>(section.get())) {
            std::cout << "  Key Not Found Policy: " << pgp->key_not_found_policy << "\n";
        } else if (auto* smime = dynamic_cast<SmimeEncryptionSection*>(section.get())) {
            std::cout << "  Key Not Found Policy: " << smime->key_not_found_policy << "\n";
        } else if (auto* pdf = dynamic_cast<PdfEncryptionSection*>(section.get())) {
            std::cout << "  Email Body Replacement: " << pdf->email_body_replacement << "\n";
            std::cout << "  PDF Main Page If Missing: " << pdf->pdf_main_page_if_missing << "\n";
            std::cout << "  PDF Password: " << pdf->pdf_password << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "=== Match Testing ===\n";
    
    // Test cases for the find_match functionality
    std::vector<std::string> testValues = {
        "user@company.com",
        "admin@internal.org", 
        "client@external.com",
        "newsletter@public.org",
        "support@example.com"
    };
    
    for (const auto& testValue : testValues) {
        std::cout << "Looking for match for: '" << testValue << "' -> ";
        
        auto* matchedSection = config.find_match(testValue);
        if (matchedSection) {
            std::cout << "Found in section [" << matchedSection->sectionName 
                      << "] (protocol: " << matchedSection->encryption_protocol << ")\n";
        } else {
            std::cout << "No match found\n";
        }
    }

    return 0;
}