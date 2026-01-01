#include "cfg/cfg.hpp"
#include "cfg2/config_manager.hpp"
#include "logger/logger.hpp"
#include "logger/spdlog_init.hpp"
#include "milter/milter.hpp"
#include "signal_manager.hpp"
#include "utils/string.hpp"
#include <boost/exception/diagnostic_information.hpp>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <fmt/core.h>
#include <grp.h>
#include <iostream>
#include <libmilter/mfapi.h>
#include <pwd.h>
#include <string>
#include <unistd.h>

using namespace std;
using namespace gwmilter;

static void print_help()
{
    cout << "\ngwmilter\n\n"
            "Options:\n"
            "  -h    This message\n"
            "  -c    Path to configuration file\n"
         << endl;
}

void drop_privileges(const std::string &user_name, const std::string &group_name)
{
    if (int uid = getuid(); uid != 0) {
        spdlog::warn("Not dropping privileges as uid is {}, not 0", uid);
        return;
    }

    if (!group_name.empty()) {
        errno = 0;
        group *g = getgrnam(group_name.c_str());

        if (g == nullptr) {
            if (errno == 0)
                throw runtime_error("Group " + group_name + " not found");
            else
                throw runtime_error(fmt::format("getgrnam() failed: {}", utils::string::str_err(errno)));
        }

        if (setgid(g->gr_gid) != 0)
            throw runtime_error(fmt::format("setgid() failed: {}", utils::string::str_err(errno)));
    }

    if (!user_name.empty()) {
        errno = 0;
        passwd *p = getpwnam(user_name.c_str());

        if (p == nullptr) {
            if (errno == 0)
                throw runtime_error(fmt::format(R"(User "{}" does not exist)", user_name));
            else
                throw runtime_error(fmt::format("getpwnam() failed: {}", utils::string::str_err(errno)));
        }

        // Reset supplementary groups
        if (setgroups(0, nullptr) != 0)
            throw runtime_error(fmt::format("setgroups() failed: {}", utils::string::str_err(errno)));

        if (setuid(p->pw_uid) != 0)
            throw runtime_error(fmt::format("setuid() failed: {}", utils::string::str_err(errno)));
    }

    spdlog::info("Privileges dropped to uid:{}, gid:{}", user_name, group_name);
}


int main(int argc, char *argv[])
{
    int ch = 0;
    const char *config_file = nullptr;

    if (argc == 1) {
        print_help();
        return EXIT_FAILURE;
    }

    while ((ch = getopt(argc, argv, "hc:")) != -1) {
        switch (ch) {
        case 'c':
            config_file = optarg;
            break;
        case 'h':
        case '?':
        default:
            print_help();
            return EXIT_FAILURE;
        }
    }

    try {
        // seed random number generator
        srand(time(nullptr));

        // Initialize cfg2 configuration system
        auto config_mgr = cfg2::ConfigManager(config_file);
        const auto config = config_mgr.getConfig();
        assert(config != nullptr);
        const auto &general_cfg = config->general;

        // Initialize logging from cfg2
        logging::init_spdlog(general_cfg);

        // Initialize legacy configuration (for backward compatibility during migration)
        cfg::cfg::inst().init(config_file);

        if (general_cfg.daemonize) {
            if (daemon(0, 0) == -1) {
                cerr << "daemon() call failed: " << utils::string::str_err(errno);
                return EXIT_FAILURE;
            }
        }

        drop_privileges(general_cfg.user, general_cfg.group);

        // Install signal handling with cfg2 reload support
        SignalManager signal_manager(config_mgr);

        spdlog::info("gwmilter starting");
        gwmilter::milter m(general_cfg.milter_socket,
                           SMFIF_ADDHDRS | SMFIF_CHGHDRS | SMFIF_CHGBODY | SMFIF_ADDRCPT | SMFIF_ADDRCPT_PAR |
                               SMFIF_DELRCPT | SMFIF_QUARANTINE | SMFIF_CHGFROM | SMFIF_SETSYMLIST,
                           general_cfg.milter_timeout);
        m.run();

        spdlog::info("gwmilter shutting down");
        return EXIT_SUCCESS;
    } catch (const cfg::cfg_exception &e) {
        spdlog::error("Configuration file error: {}", e.what());
    } catch (const boost::exception &e) {
        spdlog::error("Boost exception caught: {}", boost::diagnostic_information(e));
    } catch (const exception &e) {
        spdlog::error("Exception caught: {}", e.what());
    } catch (...) {
        spdlog::error("Unknown exception caught");
    }

    return EXIT_FAILURE;
}
