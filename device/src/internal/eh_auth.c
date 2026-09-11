#include "eh_auth.h"

#include <stdio.h>
#include <string.h>

#include "eh_hmac.h"
#include "eh_http_retry.h"
#include "eh_json.h"
#include "eh_log.h"

static eh_status_t append_content_type(char *headers, size_t cap) {
  int n = snprintf(headers, cap, "Content-Type: application/json\r\n");
  if (n <= 0 || (size_t)n >= cap) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static eh_status_t add_hmac_header(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t cap,
  int use_previous
) {
  char hex[65];
  size_t len = strlen(headers);
  int n;
  eh_status_t rc;
  const char *secret;

  if (use_previous) {
    if (!cli->has_secret_previous) return EH_ERR_AUTH;
    secret = cli->secret_previous;
  } else {
    if (!cli->has_secret_current) return EH_ERR_AUTH;
    secret = cli->secret_current;
  }
  rc = eh_hmac_sha256_hex(secret, canonical_payload, hex);
  if (rc != EH_OK) return rc;

  n = snprintf(headers + len, cap - len, "x-edgehub-device-signature: %s\r\n", hex);
  if (n <= 0 || (size_t)n >= (cap - len)) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static eh_status_t build_signed_headers(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t cap,
  int use_previous
) {
  eh_status_t rc;
  if (!cli || !headers || cap == 0) return EH_ERR_INVALID_ARG;
  headers[0] = '\0';
  rc = append_content_type(headers, cap);
  if (rc != EH_OK) return rc;
  return add_hmac_header(cli, canonical_payload, headers, cap, use_previous);
}

eh_status_t eh_build_checkin_headers(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t headers_cap
) {
  return eh_build_telemetry_headers(cli, canonical_payload, headers, headers_cap);
}

eh_status_t eh_build_telemetry_headers(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t headers_cap
) {
  return build_signed_headers(cli, canonical_payload, headers, headers_cap, 0);
}

eh_status_t eh_build_executed_headers(
  eh_client_t *cli,
  const char *canonical_payload,
  char *headers,
  size_t headers_cap
) {
  return eh_build_telemetry_headers(cli, canonical_payload, headers, headers_cap);
}

static int credential_invalid_body(int http_status, const char *resp) {
  if (http_status != 401 || !resp) return 0;
  return eh_json_has_token(resp, "DEVICE_CREDENTIAL_INVALID") ? 1 : 0;
}

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
) {
  char headers[EH_MAX_HEADERS];
  eh_status_t rc;
  int http_ok;
  int status_local = 0;
  uint32_t retry_local = 0;
  size_t n = 0;

  if (!cli) return 0;
  if (http_status) {
    *http_status = 0;
  }
  if (resp_len) {
    *resp_len = 0;
  }

  if (sign_current && canonical && canonical[0] && cli->has_secret_current) {
    rc = build_signed_headers(cli, canonical, headers, sizeof(headers), 0);
    if (rc != EH_OK) return 0;
  } else {
    rc = append_content_type(headers, sizeof(headers));
    if (rc != EH_OK) return 0;
  }

  http_ok = eh_client_http_post(
    cli, url, headers, body, body_len, resp, resp_cap, &n, &status_local, &retry_local
  );
  if (resp && resp_cap) {
    if (n >= resp_cap) n = resp_cap - 1;
    resp[n] = '\0';
  }
  if (resp_len) {
    *resp_len = n;
  }
  if (http_status) {
    *http_status = status_local;
  }
  if (retry_after_sec) {
    *retry_after_sec = retry_local;
  }
  if (!http_ok) return 0;

  if (
    credential_invalid_body(status_local, resp ? (const char *)resp : NULL) &&
    cli->has_secret_previous &&
    canonical &&
    canonical[0]
  ) {
    eh_logf(cli, EH_LOG_WARN, "hmac_retry_previous");
    rc = build_signed_headers(cli, canonical, headers, sizeof(headers), 1);
    if (rc != EH_OK) return http_ok;
    n = 0;
    status_local = 0;
    retry_local = 0;
    http_ok = eh_client_http_post(
      cli, url, headers, body, body_len, resp, resp_cap, &n, &status_local, &retry_local
    );
    if (resp && resp_cap) {
      if (n >= resp_cap) n = resp_cap - 1;
      resp[n] = '\0';
    }
    if (resp_len) {
      *resp_len = n;
    }
    if (http_status) {
      *http_status = status_local;
    }
    if (retry_after_sec) {
      *retry_after_sec = retry_local;
    }
  }
  return http_ok;
}
