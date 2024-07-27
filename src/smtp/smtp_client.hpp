#pragma once
#include "handlers/headers.hpp"
#include <curl/curl.h>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace gwmilter::smtp {

// RAII libcurl initializer
class sys_initializer {
public:
    // Init
    sys_initializer();

    // Cleanup
    ~sys_initializer();
};


class work_item {
public:
    explicit work_item(const std::string &url);

    void set_sender(const std::string &s) const;
    void set_recipients(const std::set<std::string> &rcpts) const;
    void set_message(const headers_type &headers, const std::shared_ptr<std::string> &body) const;
    CURL *get_curl_handle() const;

private:
    static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *ud);

private:
    // CURL wrapper
    struct internals_type {
        internals_type()
            : rcpt_list{nullptr}, pos{0}, err_buf{}
        {
            if ((curl = curl_easy_init()) == nullptr)
                throw std::runtime_error("curl_easy_init() failure");
        }

        ~internals_type()
        {
            curl_slist_free_all(rcpt_list);
            curl_easy_cleanup(curl);
        }

        CURL *curl;
        curl_slist *rcpt_list;
        std::string url;
        std::string sender;
        std::string headers;
        std::shared_ptr<std::string> body;
        size_t pos;
        char err_buf[CURL_ERROR_SIZE + 1];
    };

    std::shared_ptr<internals_type> internals_;
};


class client_multi {
public:
    explicit client_multi(time_t timeout);
    ~client_multi();
    client_multi(const client_multi &) = delete;
    client_multi &operator=(const client_multi &) = delete;

    void add(const work_item &wi);
    int perform();

private:
    CURLM *curlm_;
    std::vector<CURL *> curl_handles_;
    time_t timeout_;
    int running_handles_;
};

} // end namespace gwmilter::smtp
