#ifndef EH_HTTP_RETRY_H
#define EH_HTTP_RETRY_H

#include "../../include/edgehub_device.h"

static inline void eh_client_note_http_retry(eh_client_t *cli, int http_status, uint32_t retry_after_sec)
{
  if (!cli) {
    return;
  }
  cli->last_retry_after_sec = 0;
  (void)http_status;
  if (retry_after_sec > 0) {
    cli->last_retry_after_sec = retry_after_sec;
  }
}

/**
 * Dispatches HTTP POST via optional `http_post_ex` (Retry-After) or ABI-1 `http_post`.
 * Returns the callback's success flag (non-zero = transport ok).
 */
static inline int eh_client_http_post(
  eh_client_t *cli,
  const char *url,
  const char *headers,
  const uint8_t *body,
  size_t body_len,
  uint8_t *resp,
  size_t resp_cap,
  size_t *resp_len,
  int *http_status,
  uint32_t *retry_after_sec)
{
  uint32_t retry_local = 0;
  int ok;

  if (retry_after_sec) {
    *retry_after_sec = 0;
  }
  if (!cli) {
    return 0;
  }
  if (cli->cb.http_post_ex) {
    ok = cli->cb.http_post_ex(
      cli->cb.user_ctx, url, headers, body, body_len,
      resp, resp_cap, resp_len, http_status, &retry_local);
    if (retry_after_sec) {
      *retry_after_sec = retry_local;
    }
    return ok;
  }
  if (!cli->cb.http_post) {
    return 0;
  }
  return cli->cb.http_post(
    cli->cb.user_ctx, url, headers, body, body_len,
    resp, resp_cap, resp_len, http_status);
}

#endif /* EH_HTTP_RETRY_H */
