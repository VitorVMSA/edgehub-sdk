#ifndef EH_AUTH_H
#define EH_AUTH_H

#include "../../include/edgehub_device.h"

eh_status_t eh_build_checkin_headers(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t headers_cap
);

eh_status_t eh_build_telemetry_headers(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t headers_cap
);

eh_status_t eh_build_executed_headers(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t headers_cap
);

/**
 * POST com HMAC. `sign_current` 1 = assina com secret actual.
 * Se 401 `*_CREDENTIAL_INVALID` e há previous, um retry com o secret antigo.
 * `canonical` é o payload HMAC (body de check-in / canonical de telemetria/executed).
 * Devolve o flag de transporte (nonzero = OK).
 */
int eh_client_http_post_hmac(
  eh_client_t *cli,
  const char *url,
  const char *canonical,
  int sign_current,
  const uint8_t *body,
  size_t body_len,
  uint8_t *resp,
  size_t resp_cap,
  size_t *resp_len,
  int *http_status,
  uint32_t *retry_after_sec
);

#endif /* EH_AUTH_H */
