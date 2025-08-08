#include "core.hpp"
#include <filesystem>
#include <iostream>

using namespace cfg2;

int main()
{
    // Show current working directory
    std::cout << "Running in directory: " << std::filesystem::current_path() << "\n\n";

    // Load configuration from INI file
    ConfigNode root;
    try {
        root = parseIniFile("config.ini");
        std::cout << "Successfully loaded config.ini\n\n";
    } catch (const std::exception &e) {
        std::cerr << "Error loading config.ini: " << e.what() << "\n";
        std::cerr << "Using fallback hardcoded configuration...\n\n";

        // Fallback to hardcoded configuration
        root = ConfigNode{
            "config",
            "",
            {{"general",
              "",
              {{"milter_socket", "unix:/var/run/gwmilter/gwmilter.sock", {}, NodeType::VALUE},
               {"daemonize", "true", {}, NodeType::VALUE},
               {"log_type", "syslog", {}, NodeType::VALUE},
               {"log_facility", "mail", {}, NodeType::VALUE},
               {"log_priority", "info", {}, NodeType::VALUE},
               {"smtp_server", "smtp://127.0.0.1", {}, NodeType::VALUE}},
              NodeType::SECTION},
             {"pgp",
              "",
              {{"encryption_protocol", "pgp", {}, NodeType::VALUE},
               {"match", ".*@company\\.com,.*@internal\\.org", {}, NodeType::VALUE},
               {"key_not_found_policy", "retrieve", {}, NodeType::VALUE}},
              NodeType::SECTION},
             {"smime",
              "",
              {{"encryption_protocol", "smime", {}, NodeType::VALUE},
               {"match", ".*@smime\\.com", {}, NodeType::VALUE},
               {"key_not_found_policy", "reject", {}, NodeType::VALUE}},
              NodeType::SECTION},
             {"pdf",
              "",
              {{"encryption_protocol", "pdf", {}, NodeType::VALUE},
               {"match", ".*@external\\.com", {}, NodeType::VALUE},
               {"email_body_replacement", "/etc/gwmilter/pdf_body.txt", {}, NodeType::VALUE},
               {"pdf_main_page_if_missing", "/etc/gwmilter/pdf_cover.pdf", {}, NodeType::VALUE},
               {"pdf_password", "secret123", {}, NodeType::VALUE}},
              NodeType::SECTION},
             {"none",
              "",
              {{"encryption_protocol", "none", {}, NodeType::VALUE}, {"match", ".*@public\\.org", {}, NodeType::VALUE}},
              NodeType::SECTION}},
            NodeType::ROOT};
    }

    Config config = parse<Config>(root);

    std::cout << "=== General Configuration ===\n";
    std::cout << "Milter Socket: " << config.general.milter_socket << "\n";
    std::cout << "Daemonize: " << (config.general.daemonize ? "true" : "false") << "\n";
    std::cout << "Log Type: " << config.general.log_type << "\n";
    std::cout << "Log Facility: " << config.general.log_facility << "\n";
    std::cout << "Log Priority: " << config.general.log_priority << "\n";
    std::cout << "SMTP Server: " << config.general.smtp_server << "\n\n";

    std::cout << "=== Encryption Sections ===\n";
    for (const auto &section: config.encryptionSections) {
        std::cout << "[" << section->sectionName << "] (protocol: " << section->encryption_protocol << ")\n";

        if (auto *pgp = dynamic_cast<PgpEncryptionSection *>(section.get())) {
            std::cout << "  Key Not Found Policy: " << pgp->key_not_found_policy << "\n";
        } else if (auto *smime = dynamic_cast<SmimeEncryptionSection *>(section.get())) {
            std::cout << "  Key Not Found Policy: " << smime->key_not_found_policy << "\n";
        } else if (auto *pdf = dynamic_cast<PdfEncryptionSection *>(section.get())) {
            std::cout << "  Email Body Replacement: " << pdf->email_body_replacement << "\n";
            std::cout << "  PDF Main Page If Missing: " << pdf->pdf_main_page_if_missing << "\n";
            std::cout << "  PDF Password: " << pdf->pdf_password << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "=== Match Testing ===\n";

    // Test cases for the find_match functionality
    std::vector<std::string> testValues = {"user@company.com", "admin@internal.org", "client@external.com",
                                           "newsletter@public.org", "support@example.com"};

    for (const auto &testValue: testValues) {
        std::cout << "Looking for match for: '" << testValue << "' -> ";

        auto *matchedSection = config.find_match(testValue);
        if (matchedSection) {
            std::cout << "Found in section [" << matchedSection->sectionName
                      << "] (protocol: " << matchedSection->encryption_protocol << ")\n";
        } else {
            std::cout << "No match found\n";
        }
    }

    return 0;
}