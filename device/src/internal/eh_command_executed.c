#include "eh_command_executed.h"

#include <stdio.h>
#include <string.h>

#include "eh_auth.h"
#include "eh_http_retry.h"
#include "eh_json.h"
#include "eh_log.h"

static eh_status_t build_executed_url(eh_client_t *cli, const char *execution_id, char *url, size_t cap) {
  int n;

  if (cli->cfg.tenant_slug[0]) {
    n = snprintf(
      url,
      cap,
      "%s/api/v1/telemetry/public/%s/device/command-executions/%s/executed",
      cli->cfg.base_url,
      cli->cfg.tenant_slug,
      execution_id
    );
  } else {
    n = snprintf(
      url,
      cap,
      "%s/api/v1/telemetry/public/device/command-executions/%s/executed",
      cli->cfg.base_url,
      execution_id
    );
  }
  if (n <= 0 || (size_t)n >= cap) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static eh_status_t append_executed_optional_fields(
  char *out,
  size_t cap,
  size_t *len,
  int success,
  const char *message
) {
  if (message && message[0]) {
    if (!eh_json_append(out, cap, len, ",\"message\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_escape_append(out, cap, len, message)) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_append(out, cap, len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!success) {
    if (!eh_json_append(out, cap, len, ",\"success\":false")) return EH_ERR_BUFFER_TOO_SMALL;
  }
  return EH_OK;
}

static eh_status_t build_executed_body(
  eh_client_t *cli,
  int success,
  const char *message,
  char *out,
  size_t cap,
  size_t *out_len
) {
  size_t len = 0;

  if (!eh_json_append(out, cap, &len, "{\"deviceUid\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, cli->cfg.device_uid)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (append_executed_optional_fields(out, cap, &len, success, message) != EH_OK) {
    return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!eh_json_append(out, cap, &len, "}")) return EH_ERR_BUFFER_TOO_SMALL;
  *out_len = len;
  return EH_OK;
}

static eh_status_t build_executed_canonical(
  eh_client_t *cli,
  const char *execution_id,
  int success,
  const char *message,
  char *out,
  size_t cap
) {
  size_t len = 0;

  if (!eh_json_append(out, cap, &len, "{\"deviceUid\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, cli->cfg.device_uid)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\",\"alias\":\"\",\"executionId\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, execution_id)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (message && message[0]) {
    if (!eh_json_append(out, cap, &len, ",\"message\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_escape_append(out, cap, &len, message)) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_append(out, cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!success) {
    if (!eh_json_append(out, cap, &len, ",\"success\":false")) return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!eh_json_append(out, cap, &len, "}")) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

eh_status_t eh_run_command_executed(
  eh_client_t *cli,
  const char *execution_id,
  int success,
  const char *message
) {
  char url[EH_MAX_URL + 192];
  char body[EH_MAX_PAYLOAD];
  char canonical[EH_MAX_PAYLOAD];
  uint8_t resp[EH_MAX_RESPONSE];
  size_t body_len = 0;
  size_t resp_len = 0;
  int http_status = 0;
  uint32_t retry_after_sec = 0;
  eh_status_t rc;
  int http_ok;

  if (!cli || !execution_id || !execution_id[0]) return EH_ERR_INVALID_ARG;
  if (!cli->cb.http_post && !cli->cb.http_post_ex) return EH_ERR_INVALID_ARG;
  if (!cli->has_secret_current) return EH_ERR_AUTH;

  rc = build_executed_url(cli, execution_id, url, sizeof(url));
  if (rc != EH_OK) return rc;
  rc = build_executed_body(cli, success, message, body, sizeof(body), &body_len);
  if (rc != EH_OK) return rc;
  rc = build_executed_canonical(cli, execution_id, success, message, canonical, sizeof(canonical));
  if (rc != EH_OK) return rc;

  http_ok = eh_client_http_post_hmac(
    cli,
    url,
    canonical,
    1,
    (const uint8_t *)body,
    body_len,
    resp,
    sizeof(resp),
    &resp_len,
    &http_status,
    &retry_after_sec,
    NULL
  );
  if (!http_ok || http_status < 200 || http_status > 299) {
    eh_client_note_http_retry(cli, http_status, retry_after_sec);
    return EH_ERR_HTTP;
  }

  eh_logf(cli, success ? EH_LOG_INFO : EH_LOG_WARN, "command_confirm_ok");
  return EH_OK;
}
