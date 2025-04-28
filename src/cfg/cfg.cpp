#include "cfg.hpp"
#include "crypto.hpp"
#include "logger/logger.hpp"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/regex.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace gwmilter::cfg {

section_handler::section_handler(std::string name, boost::property_tree::ptree &pt)
    : section_name_(std::move(name)), pt_(pt)
{
    // this maps several values to "0" and "1", which will be
    // converted to int or bool by means of T get<T>()
    bool_map_ = {{"true", "1"}, {"on", "1"}, {"false", "0"}, {"off", "0"}};
}


void section_handler::validate()
{
    using boost::algorithm::to_lower_copy;
    using boost::property_tree::ptree;

    std::set<std::string> existing_opts;

    for (const auto &[key, value]: pt_) {
        std::string name = to_lower_copy(key);
        if (auto it = option_handlers_.find(name); it != option_handlers_.end()) {
            it->second(value.data());
            track_mandatory_[key] = true;
            existing_opts.insert(name);
        } else {
            throw cfg_exception(fmt::format(R"(section "{}", invalid option "{}")", section_name_, name));
        }
    }

    for (const auto &[option, isPresent]: track_mandatory_)
        if (!isPresent)
            throw cfg_exception(fmt::format(R"(section "{}". missing mandatory option "{}")", section_name_, option));

    fill_defaults(existing_opts);
}


void section_handler::fill_defaults(const std::set<std::string> &opts)
{
    for (const auto &[key, handler]: option_handlers_) {
        if (opts.find(key) == opts.end()) {
            if (const auto dflt = defaults_.find(key); dflt != defaults_.end())
                handler(dflt->second);
        }
    }
}


general_section_handler::general_section_handler(const std::string &name, boost::property_tree::ptree &pt)
    : section_handler(name, pt)
{
    using boost::lexical_cast;
    using std::string;

    // starting value is false, will be set to true if option is present
    track_mandatory_["milter_socket"] = false;

    // defaults
    defaults_["daemonize"] = "false";
    defaults_["log_type"] = "console";
    defaults_["log_facility"] = "mail";
    defaults_["log_priority"] = "info";
    defaults_["milter_timeout"] = "-1"; // milter default is not changed in this case
    defaults_["smtp_server"] = "smtp://127.0.0.1";
    defaults_["smtp_server_timeout"] = "-1";
    defaults_["dump_email_on_panic"] = "false";
    defaults_["strip_headers"] = "";

    // set processors for general options
    option_handlers_["daemonize"] = [this](auto &&arg) { process_daemonize(std::forward<decltype(arg)>(arg)); };
    option_handlers_["user"] = [this](auto &&arg) { process_user(std::forward<decltype(arg)>(arg)); };
    option_handlers_["group"] = [this](auto &&arg) { process_group(std::forward<decltype(arg)>(arg)); };
    option_handlers_["log_type"] = [this](auto &&arg) { process_log_type(std::forward<decltype(arg)>(arg)); };
    log_type_map_ = {
        {"console", lexical_cast<string>(logger::console)},
        {"syslog", lexical_cast<string>(logger::syslog)},
    };
    option_handlers_["log_facility"] = [this](auto &&arg) { process_log_facility(std::forward<decltype(arg)>(arg)); };
    log_facility_map_ = {
        {"user", lexical_cast<string>(logger::facility_user)},
        {"mail", lexical_cast<string>(logger::facility_mail)},
        {"news", lexical_cast<string>(logger::facility_news)},
        {"uucp", lexical_cast<string>(logger::facility_uucp)},
        {"daemon", lexical_cast<string>(logger::facility_daemon)},
        {"auth", lexical_cast<string>(logger::facility_auth)},
        {"cron", lexical_cast<string>(logger::facility_cron)},
        {"lpr", lexical_cast<string>(logger::facility_lpr)},
        {"local0", lexical_cast<string>(logger::facility_local0)},
        {"local1", lexical_cast<string>(logger::facility_local1)},
        {"local2", lexical_cast<string>(logger::facility_local2)},
        {"local3", lexical_cast<string>(logger::facility_local3)},
        {"local4", lexical_cast<string>(logger::facility_local4)},
        {"local5", lexical_cast<string>(logger::facility_local5)},
        {"local6", lexical_cast<string>(logger::facility_local6)},
        {"local7", lexical_cast<string>(logger::facility_local7)},
    };
    option_handlers_["log_priority"] = [this](auto &&arg) { process_log_priority(std::forward<decltype(arg)>(arg)); };
    log_priority_map_ = {
        {"trace", lexical_cast<string>(logger::priority_trace)},
        {"debug", lexical_cast<string>(logger::priority_debug)},
        {"info", lexical_cast<string>(logger::priority_info)},
        {"warning", lexical_cast<string>(logger::priority_warn)},
        {"error", lexical_cast<string>(logger::priority_err)},
        {"critical", lexical_cast<string>(logger::priority_critical)},
    };
    option_handlers_["milter_socket"] = [this](auto &&arg) { process_milter_socket(std::forward<decltype(arg)>(arg)); };
    option_handlers_["milter_timeout"] = [this](auto &&arg) {
        process_milter_timeout(std::forward<decltype(arg)>(arg));
    };
    option_handlers_["smtp_server"] = [this](auto &&arg) { process_smtp_server(std::forward<decltype(arg)>(arg)); };
    option_handlers_["smtp_server_timeout"] = [this](auto &&arg) {
        process_smtp_server_timeout(std::forward<decltype(arg)>(arg));
    };
    option_handlers_["dump_email_on_panic"] = [this](auto &&arg) {
        process_dump_email_on_panic(std::forward<decltype(arg)>(arg));
    };
    option_handlers_["signing_key"] = [this](auto &&arg) { process_signing_key(std::forward<decltype(arg)>(arg)); };
    option_handlers_["strip_headers"] = [this](auto &&arg) { process_strip_headers(std::forward<decltype(arg)>(arg)); };
}


void general_section_handler::process_daemonize(const std::string &optval)
{
    const auto it = bool_map_.find(boost::to_lower_copy(optval));
    if (it == bool_map_.end())
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "daemonize" ({}))", GENERAL_SECTION, optval));

    options_["daemonize"] = it->second;
}


void general_section_handler::process_user(const std::string &optval)
{
    options_["user"] = optval;
}


void general_section_handler::process_group(const std::string &optval)
{
    options_["group"] = optval;
}


void general_section_handler::process_log_type(const std::string &optval)
{
    const auto it = log_type_map_.find(boost::to_lower_copy(optval));
    if (it == log_type_map_.end())
        throw cfg_exception(fmt::format(R"(section "{}", invalid value for "log_type" ({}))", GENERAL_SECTION, optval));

    options_["log_type"] = it->second;
}

void general_section_handler::process_log_facility(const std::string &optval)
{
    const auto it = log_facility_map_.find(boost::to_lower_copy(optval));
    if (it == log_facility_map_.end())
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "log_facility" ({}))", GENERAL_SECTION, optval));

    options_["log_facility"] = it->second;
}


void general_section_handler::process_log_priority(const std::string &optval)
{
    const auto it = log_priority_map_.find(boost::to_lower_copy(optval));
    if (it == log_priority_map_.end())
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "log_priority" ({}))", GENERAL_SECTION, optval));

    options_["log_priority"] = it->second;
}


void general_section_handler::process_milter_socket(const std::string &optval)
{
    // TODO: is validation needed here? Milter does it too.
    options_["milter_socket"] = optval;
}


void general_section_handler::process_milter_timeout(const std::string &optval)
{
    try {
        // test conversion to unsigned int
        boost::lexical_cast<unsigned int>(optval);
        options_["milter_timeout"] = optval;
    } catch (const boost::bad_lexical_cast &e) {
        throw cfg_exception(fmt::format(R"(section "{}", invalid value for "milter_timeout" ({}). Integer expected.)",
                                        GENERAL_SECTION, optval));
    }
}


void general_section_handler::process_smtp_server(const std::string &optval)
{
    // TODO: validate
    options_["smtp_server"] = optval;
}


void general_section_handler::process_smtp_server_timeout(const std::string &optval)
{
    try {
        // test conversion to unsigned int
        boost::lexical_cast<unsigned int>(optval);
        options_["smtp_server_timeout"] = optval;
    } catch (const boost::bad_lexical_cast &e) {
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "smtp_server_timeout" ({}). Integer expected.)",
                        GENERAL_SECTION, optval));
    }
}


void general_section_handler::process_dump_email_on_panic(const std::string &optval)
{
    const auto it = bool_map_.find(boost::to_lower_copy(optval));
    if (it == bool_map_.end())
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "dump_email_on_panic" ({}))", GENERAL_SECTION, optval));

    options_["dump_email_on_panic"] = it->second;
}


void general_section_handler::process_signing_key(const std::string &optval)
{
    egpgcrypt::crypto c(GPGME_PROTOCOL_OpenPGP);
    if (!c.has_private_key(optval))
        throw cfg_exception(
            fmt::format(R"(section "{}", the signing key "{}" does not exist)", GENERAL_SECTION, optval));

    options_["signing_key"] = optval;
}

void general_section_handler::process_strip_headers(const std::string &optval)
{
    // store the pre-split value too
    options_["strip_headers"] = optval;
    boost::split(options_split_["strip_headers"], optval, boost::is_any_of(","));
    for (auto &v: options_split_["strip_headers"])
        boost::trim(v);
}

encryption_section_handler::encryption_section_handler(const std::string &name, boost::property_tree::ptree &pt)
    : section_handler(name, pt)
{
    using boost::lexical_cast;
    using std::string;

    track_mandatory_ = {{"match", false}, {"encryption_protocol", false}};
    option_handlers_["match"] = [this](auto &&arg) { process_match(std::forward<decltype(arg)>(arg)); };
    option_handlers_["encryption_protocol"] = [this](auto &&arg) {
        process_encryption_protocol(std::forward<decltype(arg)>(arg));
    };
    encryption_protocol_map_ = {
        {"pgp", lexical_cast<string>(pgp)},
        {"smime", lexical_cast<string>(smime)},
        {"pdf", lexical_cast<string>(pdf)},
        {"none", lexical_cast<string>(none)},
    };
}


bool encryption_section_handler::match(const std::string &recipient)
{
    return any_of(match_.begin(), match_.end(),
                  [&recipient](const auto &it) { return boost::regex_search(recipient, it); });
}


void encryption_section_handler::process_match(const std::string &optval)
{
    std::vector<std::string> split_opts;
    boost::split(split_opts, optval, boost::is_any_of(","));
    for (const auto &opt: split_opts) {
        try {
            match_.emplace(boost::trim_copy(opt));
        } catch (const boost::regex_error &e) {
            throw cfg_exception(
                fmt::format(R"(section "{}", invalid value for "match" ({}): {})", section_name_, opt, e.what()));
        }
    }
}


void encryption_section_handler::process_encryption_protocol(const std::string &optval)
{
    const auto it = encryption_protocol_map_.find(boost::to_lower_copy(optval));
    if (it == encryption_protocol_map_.end())
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "encryption_protocol" ({}))", section_name_, optval));

    options_["encryption_protocol"] = it->second;
}


pgpsmime_section_handler::pgpsmime_section_handler(const std::string &name, boost::property_tree::ptree &pt)
    : encryption_section_handler(name, pt)
{
    using boost::lexical_cast;
    using std::string;

    track_mandatory_["key_not_found_policy"] = false;

    option_handlers_["key_not_found_policy"] = [this](auto &&arg) {
        process_key_not_found_policy(std::forward<decltype(arg)>(arg));
    };

    key_not_found_policy_map_ = {
        {"discard", lexical_cast<string>(discard)},
        {"reject", lexical_cast<string>(reject)},
        {"retrieve", lexical_cast<string>(retrieve)},
    };
}


void pgpsmime_section_handler::process_key_not_found_policy(const std::string &optval)
{
    auto val = boost::to_lower_copy(optval);
    const auto it = key_not_found_policy_map_.find(val);

    if (it == key_not_found_policy_map_.end())
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "key_not_found_policy" ({}))", section_name_, optval));

    if (val == "retrieve" && get<encryption_protocol_enum>("encryption_protocol") == smime)
        throw cfg_exception(
            fmt::format(R"(section "{}", "key_not_found_policy" cannot be "retrieve" for S/MIME)", section_name_));

    options_["key_not_found_policy"] = it->second;
}


pdf_section_handler::pdf_section_handler(const std::string &name, boost::property_tree::ptree &pt)
    : encryption_section_handler(name, pt)
{
    track_mandatory_["email_body_replacement"] = false;
    track_mandatory_["pdf_main_page_if_missing"] = false;

    defaults_["pdf_attachment"] = "email.pdf";
    defaults_["pdf_font_path"] = "";
    defaults_["pdf_font_size"] = "10.0";
    defaults_["pdf_margin"] = "10.0";

    option_handlers_["pdf_attachment"] = [this](auto &&arg) {
        process_pdf_attachment(std::forward<decltype(arg)>(arg));
    };
    option_handlers_["pdf_password"] = [this](auto &&arg) { process_pdf_password(std::forward<decltype(arg)>(arg)); };
    option_handlers_["email_body_replacement"] = [this](auto &&arg) {
        process_email_body_replacement(std::forward<decltype(arg)>(arg));
    };
    option_handlers_["pdf_main_page_if_missing"] = [this](auto &&arg) {
        process_pdf_main_page_if_missing(std::forward<decltype(arg)>(arg));
    };
    option_handlers_["pdf_font_path"] = [this](auto &&arg) { process_pdf_font_path(std::forward<decltype(arg)>(arg)); };
    option_handlers_["pdf_font_size"] = [this](auto &&arg) { process_pdf_font_size(std::forward<decltype(arg)>(arg)); };
    option_handlers_["pdf_margin"] = [this](auto &&arg) { process_pdf_margin(std::forward<decltype(arg)>(arg)); };
}


void pdf_section_handler::process_pdf_attachment(const std::string &optval)
{
    // TODO: probably some escaping is necessary
    options_["pdf_attachment"] = optval;
}


void pdf_section_handler::process_pdf_password(const std::string &optval)
{
    // TODO: add support for generating random passwords
    // and email them to pre-defined email address, or store
    // in file (?)
    options_["pdf_password"] = optval;
}


void pdf_section_handler::process_email_body_replacement(const std::string &optval)
{
    if (!optval.empty() && !file_test(optval))
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "email_body_replacement" ("{}"): file does not exist)",
                        section_name_, optval));

    options_["email_body_replacement"] = optval;
}


void pdf_section_handler::process_pdf_main_page_if_missing(const std::string &optval)
{
    if (!optval.empty() && !file_test(optval))
        throw cfg_exception(
            fmt::format(R"(section "{}", invalid value for "pdf_main_page_if_missing" ("{}"): file does not exist)",
                        section_name_, optval));

    options_["pdf_main_page_if_missing"] = optval;
}


void pdf_section_handler::process_pdf_font_path(const std::string &optval)
{
    if (!optval.empty() && !file_test(optval))
        throw cfg_exception(fmt::format(
            R"(section "{}", invalid value for "pdf_font_path" ("{}"): file does not exist)", section_name_, optval));

    options_["pdf_font_path"] = optval;
}


void pdf_section_handler::process_pdf_font_size(const std::string &optval)
{
    options_["pdf_font_size"] = optval;
}


void pdf_section_handler::process_pdf_margin(const std::string &optval)
{
    options_["pdf_margin"] = optval;
}


bool pdf_section_handler::file_test(const std::string &file_path)
{
    try {
        std::filesystem::path fsPath(file_path);
        if (!exists(fsPath))
            return false;

        auto permissions = status(fsPath).permissions();
        return (permissions & std::filesystem::perms::owner_read) != std::filesystem::perms::none;
    } catch (const std::filesystem::filesystem_error &e) {
        throw cfg_exception(fmt::format("File permissions check failed: {}", e.what()));
    }
}


none_section_handler::none_section_handler(const std::string &name, boost::property_tree::ptree &pt)
    : encryption_section_handler(name, pt)
{ }


void cfg::init(const std::string &filename)
{
    try {
        boost::property_tree::read_ini(filename, pt_);

        for (const auto &[key, node]: pt_) {
            if (node.empty() && !node.data().empty())
                throw cfg_exception(
                    fmt::format("configuration option outside of a section: {} = {}", key, node.data()));

            auto section = make_section(key, pt_);
            if (!section)
                throw cfg_exception(fmt::format("make_section() failed for section \"{}\"", key));

            section->validate();

            if (boost::iequals(key, GENERAL_SECTION))
                general_section_ = section;
            else if (const auto tmp = std::dynamic_pointer_cast<encryption_section_handler>(section); tmp != nullptr)
                encryption_sections_.emplace_back(key, tmp);
            else
                throw cfg_exception("dynamic_pointer_cast<>() failed");
        }

        if (general_section_ == nullptr)
            throw cfg_exception("missing \"general\" section");
        if (encryption_sections_.empty())
            throw cfg_exception("missing encryption sections");
    } catch (const boost::property_tree::ptree_error &e) {
        // convert boost::property_tree exceptions to cfg_exception
        throw cfg_exception(e.what());
    }
}


std::shared_ptr<section_handler> cfg::section(const std::string &sectionname) const
{
    if (boost::iequals(sectionname, GENERAL_SECTION))
        return general_section_;

    auto it = find_if(encryption_sections_.begin(), encryption_sections_.end(),
                      [&sectionname](const auto &pair) { return pair.first == sectionname; });

    if (it == encryption_sections_.end())
        throw cfg_exception(fmt::format("section \"{}\" does not exist", sectionname));

    return it->second;
}


std::shared_ptr<encryption_section_handler> cfg::find_match(const std::string &recipient) const
{
    for (const auto &it: encryption_sections_)
        if (it.second->match(recipient))
            return it.second;

    return nullptr;
}


std::shared_ptr<section_handler> cfg::make_section(const std::string &name, boost::property_tree::ptree &pt)
{
    boost::property_tree::ptree node = pt.get_child(name);

    if (!node.empty()) {
        if (boost::iequals(name, GENERAL_SECTION))
            return std::make_shared<general_section_handler>(name, node);

        auto protocol = node.get_optional<std::string>("encryption_protocol");
        if (!protocol)
            throw cfg_exception(fmt::format("section \"{}\" missing encryption_protocol", name));

        if (boost::iequals(protocol.value(), "pgp") || boost::iequals(protocol.value(), "smime"))
            return std::make_shared<pgpsmime_section_handler>(name, node);
        else if (boost::iequals(protocol.value(), "pdf"))
            return std::make_shared<pdf_section_handler>(name, node);
        else if (boost::iequals(protocol.value(), "none"))
            return std::make_shared<none_section_handler>(name, node);
        else
            throw cfg_exception(
                fmt::format(R"(section "{}", encryption_protocol has invalid value ("{}"))", name, protocol.value()));
    } else {
        throw cfg_exception(fmt::format("Malformed section: {}", name));
    }
}

} // namespace gwmilter::cfg
