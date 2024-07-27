#include "smtp_client.hpp"
#include "logger/logger.hpp"
#include "utils/string.hpp"
#include <cerrno>
#include <cstring>
#include <curl/curl.h>
#include <iostream>
#include <memory>
#include <set>
#include <string>

namespace gwmilter::smtp {

static gwmilter::smtp::sys_initializer curl;


sys_initializer::sys_initializer()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}


sys_initializer::~sys_initializer()
{
    curl_global_cleanup();
}


work_item::work_item(const std::string &url)
    : internals_{std::make_shared<internals_type>()}
{
    internals_->url = url;
    curl_easy_setopt(internals_->curl, CURLOPT_URL, internals_->url.c_str());
    curl_easy_setopt(internals_->curl, CURLOPT_NOSIGNAL, 1L);
    // curl_easy_setopt(internals_->curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(internals_->curl, CURLOPT_ERRORBUFFER, internals_->err_buf);
    curl_easy_setopt(internals_->curl, CURLOPT_READFUNCTION, work_item::read_callback);
    curl_easy_setopt(internals_->curl, CURLOPT_READDATA, internals_.get());
    curl_easy_setopt(internals_->curl, CURLOPT_UPLOAD, 1L);
}


void work_item::set_sender(const std::string &s) const
{
    internals_->sender = s;
    curl_easy_setopt(internals_->curl, CURLOPT_MAIL_FROM, internals_->sender.c_str());
}


void work_item::set_recipients(const std::set<std::string> &rcpts) const
{
    for (const auto &rcpt: rcpts)
        internals_->rcpt_list = curl_slist_append(internals_->rcpt_list, rcpt.c_str());

    curl_easy_setopt(internals_->curl, CURLOPT_MAIL_RCPT, internals_->rcpt_list);
}


void work_item::set_message(const headers_type &headers, const std::shared_ptr<std::string> &body) const
{
    internals_->body = body;

    // for simplicity, create a single buffer for headers
    for (const auto &header: headers) {
        // only add headers that are not marked as deleted
        if (!(header.modified && header.value.empty()))
            internals_->headers += header.name + ": " + header.value + "\r\n";
    }
    internals_->headers += "\r\n";
}


CURL *work_item::get_curl_handle() const
{
    return internals_->curl;
}


size_t work_item::read_callback(void *ptr, size_t size, size_t nmemb, void *ud)
{
    auto *wi = reinterpret_cast<internals_type *>(ud);

    if (size * nmemb <= 0)
        return 0;

    size_t &pos = wi->pos;
    const std::size_t headers_size = wi->headers.size();
    const std::size_t body_size = wi->body->size();

    if (pos < headers_size) {
        size_t s = std::min(size * nmemb, headers_size - pos);
        memcpy(ptr, wi->headers.c_str() + pos, s);
        pos += s;

        return s;
    } else if (pos < headers_size + body_size) {
        size_t s = std::min(size * nmemb, headers_size + body_size - pos);
        size_t bp = pos - headers_size;
        memcpy(ptr, wi->body->c_str() + bp, s);
        pos += s;

        return s;
    }

    return 0;
}


client_multi::client_multi(time_t timeout)
    : curlm_{curl_multi_init()}, timeout_{timeout}, running_handles_{0}
{
    if (curlm_ == nullptr)
        throw std::runtime_error("curl_multi_init() failed");
}


client_multi::~client_multi()
{
    CURLMcode curlm_code;

    for (const auto &curl_handle: curl_handles_)
        if ((curlm_code = curl_multi_remove_handle(curlm_, curl_handle)) != CURLM_OK)
            L_ERR << "curl_multi_remove_handle() failed: " << curl_multi_strerror(curlm_code);

    if ((curlm_code = curl_multi_cleanup(curlm_)) != CURLM_OK)
        L_ERR << "curl_multi_remove_handle() failed: " << curl_multi_strerror(curlm_code);
}


void client_multi::add(const work_item &wi)
{
    curl_handles_.push_back(wi.get_curl_handle());

    if (timeout_ != -1)
        curl_easy_setopt(wi.get_curl_handle(), CURLOPT_TIMEOUT, timeout_);

    CURLMcode curlm_code;
    if ((curlm_code = curl_multi_add_handle(curlm_, wi.get_curl_handle())) != CURLM_OK)
        throw std::runtime_error(std::string("curl_multi_add_handle() failed: ") + curl_multi_strerror(curlm_code));

    ++running_handles_;
}


int client_multi::perform()
{
    // TODO: improve handling of CURLM (ie. timeouts, error handling...)
    while (running_handles_ != 0) {
        int maxfd = -1;
        long curl_timeout = -1;
        fd_set rfd, wfd, efd;
        FD_ZERO(&rfd);
        FD_ZERO(&wfd);
        FD_ZERO(&efd);

        timeval to{};
        to.tv_sec = 1;
        to.tv_usec = 0;

        // get libcurl's recommended timeout value
        CURLMcode curlm_code = curl_multi_timeout(curlm_, &curl_timeout);
        if (curlm_code != CURLM_OK)
            throw std::runtime_error(std::string("curl_multi_timeout() failed: ") + curl_multi_strerror(curlm_code));

        if (curl_timeout >= 0) {
            to.tv_sec = curl_timeout / 1000;
            if (to.tv_sec > 1)
                to.tv_sec = 1;
            else
                to.tv_usec = (curl_timeout % 1000) * 1000;
        } // else, using the default (set above)

        curlm_code = curl_multi_fdset(curlm_, &rfd, &wfd, &efd, &maxfd);
        if (curlm_code != CURLM_OK)
            throw std::runtime_error(std::string("curl_multi_fdset() failed: ") + curl_multi_strerror(curlm_code));

        int rc = select(maxfd + 1, &rfd, &wfd, &efd, &to);
        if (rc == -1)
            throw std::runtime_error(std::string("select() failed: ") + utils::string::str_err(errno));

        curlm_code = curl_multi_perform(curlm_, &running_handles_);
        if (curlm_code != CURLM_OK) {
            throw std::runtime_error(std::string("curl_multi_perform() failed: return code = ") +
                                     curl_multi_strerror(curlm_code));
        }
    }

    int msgs_in_queue = 0;
    int failed_count = 0;
    for (CURLMsg *msg = curl_multi_info_read(curlm_, &msgs_in_queue); msg != nullptr;
         msg = curl_multi_info_read(curlm_, &msgs_in_queue))
    {
        if (msg->msg == CURLMSG_DONE) {
            if (msg->data.result != CURLE_OK) {
                long resp_code = 0;
                long os_errno = 0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &resp_code);
                curl_easy_getinfo(msg->easy_handle, CURLINFO_OS_ERRNO, &os_errno);
                L_ERR << "SMTP worker failed (response_code=" << resp_code << ", errno=" << os_errno
                      << ", err=" << utils::string::str_err(os_errno) << ")";

                ++failed_count;
            }
        }
    }

    return failed_count;
}

} // end namespace gwmilter::smtp
