#pragma once
#include "logger/logger.hpp"
#include "milter_callbacks.hpp"
#include "milter_message.hpp"
#include <cassert>
#include <libmilter/mfapi.h>
#include <string>

namespace gwmilter {

class milter_connection {
public:
    explicit milter_connection(SMFICTX *ctx)
        : smfictx_{ctx}, connection_id_{uid_gen_.generate()}
    { }
    milter_connection(const milter_connection &) = delete;
    milter_connection &operator=(const milter_connection &) = delete;

    sfsistat on_connect(const std::string &hostname, _SOCK_ADDR *hostaddr);
    sfsistat on_helo(const std::string &helohost);
    sfsistat on_close();
    sfsistat on_eom();
    sfsistat on_abort();
    sfsistat on_unknown(const std::string &arg);

    std::shared_ptr<milter_message> get_message()
    {
        if (msg_ == nullptr) {
            // Normally this is only expected to happen on `xxfi_envfrom` callback,
            // but for extra safety `milter_message` is initialized whenever it is nullptr.
            spdlog::debug("{}: get_message() creating new milter_message object", connection_id_);
            msg_ = std::make_shared<milter_message>(smfictx_, connection_id_, callbacks::get_config());
        }
        return msg_;
    }

private:
    static std::string hostaddr_to_string(_SOCK_ADDR *hostaddr);

private:
    SMFICTX *smfictx_;
    uid_generator uid_gen_;
    std::string connection_id_;
    std::shared_ptr<milter_message> msg_;
};

} // end namespace gwmilter
