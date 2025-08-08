#include "core.hpp"
#include <iostream>

using namespace cfg2;

int main() {
    // Example INI structure representing:
    // [general]
    // appName=MyApp
    // version=1.0.0
    // logLevel=2
    //
    // [db1]
    // type=database
    // match=^user_.*$,^admin_.*$
    // host=localhost
    // port=5432
    // username=admin
    // password=secret
    //
    // [cache1]
    // type=cache
    // match=^session_.*$, ^temp_.*$
    // redisHost=redis.example.com
    // redisPort=6379
    // ttl=3600
    //
    // [api_service]
    // type=service
    // match=^api_.*$,^webhook_.*$
    // endpoint=https://api.example.com
    // timeout=30
    // enabled=true
    
    TreeNode root{"config","",{
        {"general","",{
            {"appName","MyApp"},
            {"version","1.0.0"},
            {"logLevel","2"}
        }},
        {"db1","",{
            {"type","database"},
            {"match","^user_.*$,^admin_.*$",{}},
            {"host","localhost"},
            {"port","5432"},
            {"username","admin"},
            {"password","secret"}
        }},
        {"cache1","",{
            {"type","cache"},
            {"match","^session_.*$, ^temp_.*$",{}},
            {"redisHost","redis.example.com"},
            {"redisPort","6379"},
            {"ttl","3600"}
        }},
        {"api_service","",{
            {"type","service"},
            {"match","^api_.*$,^webhook_.*$",{}},
            {"endpoint","https://api.example.com"},
            {"timeout","30"},
            {"enabled","true"}
        }}
    }};

    IniConfig config = parse<IniConfig>(root);

    std::cout << "=== General Configuration ===\n";
    std::cout << "App Name: " << config.general.appName << "\n";
    std::cout << "Version: " << config.general.version << "\n";
    std::cout << "Log Level: " << config.general.logLevel << "\n\n";

    std::cout << "=== Dynamic Sections ===\n";
    for (const auto& section : config.dynamicSections) {
        std::cout << "[" << section->sectionName << "] (type: " << section->type << ")\n";
        
        if (auto* db = dynamic_cast<DatabaseSection*>(section.get())) {
            std::cout << "  Host: " << db->host << "\n";
            std::cout << "  Port: " << db->port << "\n";
            std::cout << "  Username: " << db->username << "\n";
        } else if (auto* cache = dynamic_cast<CacheSection*>(section.get())) {
            std::cout << "  Redis Host: " << cache->redisHost << "\n";
            std::cout << "  Redis Port: " << cache->redisPort << "\n";
            std::cout << "  TTL: " << cache->ttl << "\n";
        } else if (auto* service = dynamic_cast<ServiceSection*>(section.get())) {
            std::cout << "  Endpoint: " << service->endpoint << "\n";
            std::cout << "  Timeout: " << service->timeout << "\n";
            std::cout << "  Enabled: " << (service->enabled ? "true" : "false") << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "=== Match Testing ===\n";
    
    // Test cases for the find_match functionality
    std::vector<std::string> testValues = {
        "user_12345",
        "admin_root", 
        "session_abc123",
        "temp_cache_key",
        "api_request_001",
        "webhook_github",
        "unknown_value"
    };
    
    for (const auto& testValue : testValues) {
        std::cout << "Looking for match for: '" << testValue << "' -> ";
        
        auto* matchedSection = config.find_match(testValue);
        if (matchedSection) {
            std::cout << "Found in section [" << matchedSection->sectionName 
                      << "] (type: " << matchedSection->type << ")\n";
        } else {
            std::cout << "No match found\n";
        }
    }

    return 0;
}