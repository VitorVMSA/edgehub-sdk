#ifndef EH_B64_H
#define EH_B64_H

#include <stddef.h>
#include <stdint.h>

#include "../include/edgehub_device.h"

eh_status_t eh_b64_decode(const char *in, uint8_t *out, size_t out_cap, size_t *out_len);
eh_status_t eh_b64url_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap, size_t *out_len);

#endif
