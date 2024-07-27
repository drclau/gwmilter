#pragma once
#include <libmilter/mfapi.h>

namespace gwmilter {

// callbacks for libmilter
sfsistat xxfi_connect(SMFICTX *ctx, char *hostname, _SOCK_ADDR *hostaddr);
sfsistat xxfi_helo(SMFICTX *ctx, char *helohost);
sfsistat xxfi_envfrom(SMFICTX *ctx, char **argv);
sfsistat xxfi_envrcpt(SMFICTX *ctx, char **argv);
sfsistat xxfi_data(SMFICTX *ctx);
sfsistat xxfi_unknown(SMFICTX *ctx, const char *arg);
sfsistat xxfi_header(SMFICTX *ctx, char *headerf, char *headerv);
sfsistat xxfi_eoh(SMFICTX *ctx);
sfsistat xxfi_body(SMFICTX *ctx, unsigned char *bodyp, size_t len);
sfsistat xxfi_eom(SMFICTX *ctx);
sfsistat xxfi_abort(SMFICTX *ctx);
sfsistat xxfi_close(SMFICTX *ctx);

} // namespace gwmilter
