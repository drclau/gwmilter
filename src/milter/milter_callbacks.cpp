#include "milter_callbacks.hpp"
#include "logger/logger.hpp"
#include "milter_connection.hpp"
#include <libmilter/mfapi.h>
#include <string>
#include <vector>

namespace gwmilter {

sfsistat xxfi_connect(SMFICTX *ctx, char *hostname, _SOCK_ADDR *hostaddr)
{
    try {
        auto *m = new milter_connection(ctx);
        smfi_setpriv(ctx, m);

        return m->on_connect(hostname, hostaddr);
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_helo(SMFICTX *ctx, char *helohost)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx)))
            return m->on_helo(helohost);
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_close(SMFICTX *ctx)
{
    try {
        sfsistat ret = SMFIS_CONTINUE;
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx))) {
            ret = m->on_close();
            delete m;
            smfi_setpriv(ctx, nullptr);
        }

        return ret;
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_envfrom(SMFICTX *ctx, char **argv)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx))) {
            std::vector<std::string> args;
            for (int i = 0; argv[i] != nullptr; ++i)
                args.emplace_back(argv[i]);

            return m->get_message()->on_envfrom(args);
        }
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_envrcpt(SMFICTX *ctx, char **argv)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx))) {
            std::vector<std::string> args;
            for (int i = 0; argv[i] != nullptr; ++i)
                args.emplace_back(argv[i]);

            return m->get_message()->on_envrcpt(args);
        }
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_data(SMFICTX *ctx)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx)))
            return m->get_message()->on_data();
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_unknown(SMFICTX *ctx, const char *arg)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx)))
            return m->on_unknown(arg);
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_header(SMFICTX *ctx, char *headerf, char *headerv)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx)))
            return m->get_message()->on_header(headerf, headerv);
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_eoh(SMFICTX *ctx)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx)))
            return m->get_message()->on_eoh();
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_body(SMFICTX *ctx, unsigned char *bodyp, size_t len)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx))) {
            const std::string body(reinterpret_cast<char *>(bodyp), len);
            return m->get_message()->on_body(body);
        }
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_eom(SMFICTX *ctx)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx)))
            return m->on_eom();
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}


sfsistat xxfi_abort(SMFICTX *ctx)
{
    try {
        if (auto *m = static_cast<milter_connection *>(smfi_getpriv(ctx)))
            return m->on_abort();
    } catch (const std::exception &e) {
        L_ERR << "std::exception caught: " << e.what();
    } catch (...) {
        L_ERR << "unknown exception caught";
    }

    return SMFIS_TEMPFAIL;
}

} // namespace gwmilter
