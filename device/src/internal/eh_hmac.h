#ifndef EH_HMAC_H
#define EH_HMAC_H

#include "../include/edgehub_device.h"

eh_status_t eh_hmac_sha256_hex(const char *secret, const char *message, char out_hex[65]);

#endif
