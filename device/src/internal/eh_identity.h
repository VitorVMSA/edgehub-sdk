#ifndef EH_IDENTITY_H
#define EH_IDENTITY_H

#include "../include/edgehub_device.h"

eh_status_t eh_ed25519_sign_utf8(const eh_client_t *cli, const char *message_utf8, char *out_b64url, size_t out_cap);

#endif
