#include "milter_connection.hpp"
#include "logger/logger.hpp"
#include <arpa/inet.h>
#include <libmilter/mfapi.h>
#include <string>

namespace gwmilter {

using std::make_shared;

sfsistat milter_connection::on_connect(const std::string &hostname, _SOCK_ADDR *hostaddr)
{
    L_INFO << connection_id_ << ": connect from hostname=" << hostname
           << ", hostaddr=" << milter_connection::hostaddr_to_string(hostaddr);
    return SMFIS_CONTINUE;
}


sfsistat milter_connection::on_helo(const std::string &helohost)
{
    L_DEBUG << connection_id_ << ": helo/ehlo host=" << helohost;
    return SMFIS_CONTINUE;
}


sfsistat milter_connection::on_close()
{
    L_INFO << connection_id_ << ": close-connection";
    msg_.reset();
    return SMFIS_CONTINUE;
}


sfsistat milter_connection::on_eom()
{
    sfsistat ret = get_message()->on_eom();
    msg_.reset();
    return ret;
}


sfsistat milter_connection::on_abort()
{
    sfsistat ret = get_message()->on_abort();
    msg_.reset();
    return ret;
}


sfsistat milter_connection::on_unknown(const std::string &arg)
{
    L_DEBUG << connection_id_ << ": unknown arg=" << arg;
    return SMFIS_CONTINUE;
}


std::string milter_connection::hostaddr_to_string(_SOCK_ADDR *hostaddr)
{
    char ip[INET6_ADDRSTRLEN] = {0};

    switch (hostaddr->sa_family) {
    case AF_INET: {
        auto *ipv4 = reinterpret_cast<const struct sockaddr_in *>(hostaddr);
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip, sizeof(ip));
        break;
    }
    case AF_INET6: {
        auto *ipv6 = reinterpret_cast<const struct sockaddr_in6 *>(hostaddr);
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip, sizeof(ip));
        break;
    }
    }

    return ip;
}

} // namespace gwmilter
