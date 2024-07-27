#pragma once
#include "utils/string.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/regex.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gwmilter::cfg {

inline const std::string GENERAL_SECTION = "general";

enum encryption_protocol_enum { none, pgp, smime, pdf };
enum key_not_found_policy_enum { discard, reject, retrieve };


class cfg_exception : public std::runtime_error {
public:
    cfg_exception()
        : runtime_error{"N/A"}
    { }

    explicit cfg_exception(const std::string &what)
        : runtime_error{what}
    { }
};


// base class for section handlers
class section_handler {
public:
    using OptionHandler = std::function<void(const std::string &)>;

    section_handler(std::string name, boost::property_tree::ptree &pt);
    virtual ~section_handler() = default;

    void validate();
    template<typename T> T get(const std::string &optname) const;
    std::string name() const;

private:
    void fill_defaults(const std::set<std::string> &opts);

protected:
    std::string section_name_;
    boost::property_tree::ptree pt_;

    // map holding the options. All values are stored as strings.
    std::map<std::string, std::string> options_;
    // map holding the options as a vector of strings
    // optimization for options that accept a list of comma separated values
    std::map<std::string, std::vector<std::string>> options_split_;

    // map of valid options and the associated handler for the value
    std::map<std::string, OptionHandler> option_handlers_;
    // map holding mandatory options
    std::map<std::string, bool> track_mandatory_;
    // map holding default values
    std::map<std::string, std::string> defaults_;

    // --- helper maps
    // map used for true/false options
    std::map<std::string, std::string> bool_map_;
};


class general_section_handler final : public section_handler {
public:
    general_section_handler(const std::string &name, boost::property_tree::ptree &pt);

private:
    void process_daemonize(const std::string &optval);
    void process_user(const std::string &optval);
    void process_group(const std::string &optval);
    void process_log_facility(const std::string &optval);
    void process_log_priority(const std::string &optval);
    void process_milter_socket(const std::string &optval);
    void process_milter_timeout(const std::string &optval);
    void process_smtp_server(const std::string &optval);
    void process_smtp_server_timeout(const std::string &optval);
    void process_dump_email_on_panic(const std::string &optval);
    void process_signing_key(const std::string &optval);
    void process_strip_headers(const std::string &optval);

private:
    // helper maps
    std::map<std::string, std::string> log_facility_map_;
    std::map<std::string, std::string> log_priority_map_;
};


class encryption_section_handler : public section_handler {
public:
    encryption_section_handler(const std::string &name, boost::property_tree::ptree &pt);
    virtual bool match(const std::string &recipient);

private:
    void process_match(const std::string &optval);
    void process_encryption_protocol(const std::string &optval);

private:
    std::set<boost::regex> match_;
    std::map<std::string, std::string> encryption_protocol_map_;
};


class pgpsmime_section_handler final : public encryption_section_handler {
public:
    pgpsmime_section_handler(const std::string &name, boost::property_tree::ptree &pt);

private:
    void process_key_not_found_policy(const std::string &optval);

private:
    std::map<std::string, std::string> key_not_found_policy_map_;
};


class pdf_section_handler final : public encryption_section_handler {
public:
    pdf_section_handler(const std::string &name, boost::property_tree::ptree &pt);

private:
    void process_pdf_attachment(const std::string &optval);
    void process_pdf_password(const std::string &optval);
    void process_pdf_body_replacement(const std::string &optval);
    void process_pdf_main_page_if_missing(const std::string &optval);
    void process_pdf_font_path(const std::string &optval);
    void process_pdf_font_size(const std::string &optval);
    void process_pdf_margin(const std::string &optval);

    static bool file_test(const std::string &path);
};


class none_section_handler final : public encryption_section_handler {
public:
    none_section_handler(const std::string &name, boost::property_tree::ptree &pt);
};


class cfg {
public:
    cfg(const cfg &) = delete;
    cfg &operator=(const cfg &) = delete;

    static cfg &inst()
    {
        static cfg c;
        return c;
    }

    void init(const std::string &filename);
    std::shared_ptr<section_handler> section(const std::string &sectionname) const;
    std::shared_ptr<encryption_section_handler> find_match(const std::string &recipient) const;

private:
    cfg() = default;

private:
    static std::shared_ptr<section_handler> make_section(const std::string &name, boost::property_tree::ptree &pt);

private:
    boost::property_tree::ptree pt_;
    // GENERAL_SECTION section gets special treatment
    std::shared_ptr<section_handler> general_section_;

    using encryption_pair_type = std::pair<std::string, std::shared_ptr<encryption_section_handler>>;
    std::vector<encryption_pair_type> encryption_sections_;
};


template<typename T> T section_handler::get(const std::string &optname) const
{
    const auto it = options_.find(optname);
    if (it == options_.end())
        throw cfg_exception("Option \"" + optname + "\" is invalid in section \"" + section_name_ + "\"");

    return boost::lexical_cast<T>(it->second);
}


// boost::lexical_cast is not aware of encryption_protocol_enum and
// key_not_found_policy_enum, hence section_handler::get() specializations
// are required to deal with them.
template<> inline encryption_protocol_enum section_handler::get(const std::string &optname) const
{
    return static_cast<encryption_protocol_enum>(get<long>(optname));
}


template<> inline key_not_found_policy_enum section_handler::get(const std::string &optname) const
{
    return static_cast<key_not_found_policy_enum>(get<long>(optname));
}


template<> inline std::vector<std::string> section_handler::get(const std::string &optname) const
{
    const auto it = options_split_.find(optname);
    if (it == options_split_.end())
        throw cfg_exception("Option \"" + optname + "\" is invalid in section \"" + section_name_ + "\"");

    return it->second;
}


inline std::string section_handler::name() const
{
    return section_name_;
}

} // namespace gwmilter::cfg
