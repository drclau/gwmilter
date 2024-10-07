#pragma once
#include "cfg/cfg.hpp"
#include "headers.hpp"
#include <crypto.hpp>
#include <data_buffers.hpp>
#include <epdf.hpp>
#include <map>
#include <mime_unpacker.hpp>
#include <set>
#include <string>
#include <vector>

namespace gwmilter {

using recipients_type = std::set<std::string>;

class body_handler_base {
public:
    body_handler_base();
    virtual ~body_handler_base() = default;

    void add_header(const std::string &headerf, const std::string &headerv);

    virtual void write(const std::string &data);
    virtual headers_type get_headers() = 0;
    virtual void encrypt(const recipients_type &recipients, std::string &out) = 0;

    virtual bool has_public_key(const std::string &recipient) const = 0;
    virtual bool import_public_key(const std::string &recipient) = 0;

    const std::set<std::string> &failed_recipients() { return expired_keys_; }

protected:
    static std::string generate_boundary(std::size_t length = 70);

    // returns value of Content-Type and fills the param with all
    // Content-* headers
    std::string extract_content_headers(headers_type &content_headers);

    void make_multipart(const std::string &content_type);
    virtual bool convert_to_multipart() const;

    virtual void preprocess();
    virtual void postprocess();

protected:
    headers_type headers_;
    // this keeps the index per header type; it is required in milter
    std::map<std::string, unsigned int> header_pos_;
    std::string multipart_boundary_;
    bool preprocessed_;
    std::set<std::string> expired_keys_;
};


class egpgcrypt_body_handler : public body_handler_base {
public:
    explicit egpgcrypt_body_handler(gpgme_protocol_t protocol);

    void write(const std::string &data) override;
    bool has_public_key(const std::string &recipient) const override;
    bool import_public_key(const std::string &recipient) override;

protected:
    egpgcrypt::crypto crypto_;
    // TODO: use file_data_buffer when size is greater than a configurable limit
    egpgcrypt::memory_data_buffer body_;
};


class pgp_body_handler final : public egpgcrypt_body_handler {
public:
    pgp_body_handler();

    headers_type get_headers() override;
    void encrypt(const recipients_type &recipients, std::string &out) override;

protected:
    bool convert_to_multipart() const override;

private:
    std::string main_boundary_;
};


class smime_body_handler final : public egpgcrypt_body_handler {
public:
    smime_body_handler();

    headers_type get_headers() override;
    void encrypt(const recipients_type &recipients, std::string &out) override;

private:
    bool new_headers_added_;
};


class pdf_body_handler final : public body_handler_base {
public:
    explicit pdf_body_handler(const std::shared_ptr<cfg::encryption_section_handler> &settings);

    void write(const std::string &data) override;
    headers_type get_headers() override;
    void encrypt(const recipients_type &recipients, std::string &out) override;
    bool has_public_key(const std::string &recipient) const override;
    bool import_public_key(const std::string &recipient) override;

protected:
    void preprocess() override;
    void postprocess() override;

private:
    static std::string read_file(const std::string &filename);

private:
    epdfcrypt::memory_mime_stream body_;
    std::string main_boundary_;
    std::string pdf_attachment_;

    std::shared_ptr<cfg::encryption_section_handler> settings_;
};


class noop_body_handler final : public body_handler_base {
public:
    explicit noop_body_handler(const std::shared_ptr<cfg::encryption_section_handler> &);

    void write(const std::string &data) override;
    headers_type get_headers() override;
    void encrypt(const recipients_type &recipients, std::string &out) override;
    bool has_public_key(const std::string &recipient) const override;
    bool import_public_key(const std::string &recipient) override;

private:
    std::string data_;
};

} // end namespace gwmilter
