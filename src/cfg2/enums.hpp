#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "utils/string.hpp"
namespace cfg2 {

// Encryption protocol types
enum class EncryptionProtocol { Pgp, Smime, Pdf, None };

// Key not found policy options
enum class KeyNotFoundPolicy { Discard, Retrieve, Reject };

// String conversion for EncryptionProtocol
template<typename T> T fromString(const std::string &str);

template<> [[nodiscard]] inline EncryptionProtocol fromString<EncryptionProtocol>(const std::string &str)
{
    const std::string lower = gwmilter::utils::string::to_lower(str);
    if (lower == "pgp")
        return EncryptionProtocol::Pgp;
    if (lower == "smime")
        return EncryptionProtocol::Smime;
    if (lower == "pdf")
        return EncryptionProtocol::Pdf;
    if (lower == "none")
        return EncryptionProtocol::None;
    throw std::invalid_argument("Invalid encryption_protocol value: " + str + " (expected: pgp, smime, pdf, none)");
}

template<> [[nodiscard]] inline KeyNotFoundPolicy fromString<KeyNotFoundPolicy>(const std::string &str)
{
    const std::string lower = gwmilter::utils::string::to_lower(str);
    if (lower == "discard")
        return KeyNotFoundPolicy::Discard;
    if (lower == "retrieve")
        return KeyNotFoundPolicy::Retrieve;
    if (lower == "reject")
        return KeyNotFoundPolicy::Reject;
    throw std::invalid_argument("Invalid key_not_found_policy value: " + str +
                                " (expected: discard, retrieve, reject)");
}

// toString functions for logging and error messages
[[nodiscard]] inline std::string_view toString(EncryptionProtocol p)
{
    switch (p) {
    case EncryptionProtocol::Pgp:
        return "pgp";
    case EncryptionProtocol::Smime:
        return "smime";
    case EncryptionProtocol::Pdf:
        return "pdf";
    case EncryptionProtocol::None:
        return "none";
    }
    __builtin_unreachable();
}

[[nodiscard]] inline std::string_view toString(KeyNotFoundPolicy p)
{
    switch (p) {
    case KeyNotFoundPolicy::Discard:
        return "discard";
    case KeyNotFoundPolicy::Retrieve:
        return "retrieve";
    case KeyNotFoundPolicy::Reject:
        return "reject";
    }
    __builtin_unreachable();
}

} // namespace cfg2
