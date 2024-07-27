#include "milter.hpp"
#include "milter_callbacks.hpp"
#include "milter_exception.hpp"
#include <libmilter/mfapi.h>

namespace gwmilter {

milter::milter(const std::string &socket, unsigned long flags, int timeout, int backlog, int debug_level)
{
    if (smfi_setconn(const_cast<char *>(socket.c_str())) == MI_FAILURE)
        throw milter_exception("smfi_setconn failed");

    if (timeout != -1 && smfi_settimeout(timeout) == MI_FAILURE)
        throw milter_exception("smfi_settimeout failed");

    if (backlog != -1 && smfi_setbacklog(backlog) == MI_FAILURE)
        throw milter_exception("smfi_setbacklog failed");

    if (debug_level != -1 && smfi_setdbg(debug_level) == MI_FAILURE)
        throw milter_exception("smfi_setdbg failed");

    smfiDesc smfilter = {
        const_cast<char *>("gwfilter"), // filter name
        SMFI_VERSION, // version code -- do not change
        flags, // flags
        xxfi_connect, // connection info filter
        xxfi_helo, // SMTP HELO command filter
        xxfi_envfrom, // envelope sender filter
        xxfi_envrcpt, // envelope recipient filter
        xxfi_header, // header filter
        xxfi_eoh, // end of header
        xxfi_body, // body block filter
        xxfi_eom, // end of message
        xxfi_abort, // message aborted
        xxfi_close, // connection cleanup
        xxfi_unknown, // unknown SMTP commands
        xxfi_data, // DATA command
        nullptr // xxfi_negotiate unused
    };

    if (smfi_register(smfilter) == MI_FAILURE)
        throw milter_exception("smfi_setdbg failed");
}


void milter::run()
{
    smfi_main();
}

} // namespace gwmilter
