#include "eh_checkin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eh_auth.h"
#include "eh_http_retry.h"
#include "eh_json.h"
#include "eh_log.h"
#include "eh_provision.h"

static int has_full_checkin_params(const eh_client_t *cli) {
  return cli->cfg.group_key[0] != '\0' && cli->cfg.product_code[0] != '\0';
}

static const char *resolved_checkin_keys_json(const eh_client_t *cli) {
  if (!cli || cli->checkin_keys_json[0] == '\0') return "{}";
  return cli->checkin_keys_json;
}

eh_status_t eh_checkin_build_keys_json(eh_client_t *cli, char *out, size_t cap) {
  eh_values_writer_t writer;
  eh_status_t rc;

  if (cli->cb.checkin_keys_collect) {
    rc = eh_values_begin(&writer, out, cap);
    if (rc != EH_OK) return rc;
    if (!cli->cb.checkin_keys_collect(cli->cb.user_ctx, &writer)) return EH_ERR_IO;
    return eh_values_end(&writer);
  }

  if (cap < 3) return EH_ERR_BUFFER_TOO_SMALL;
  snprintf(out, cap, "%s", resolved_checkin_keys_json(cli));
  return EH_OK;
}

void eh_checkin_store_sent_keys(eh_client_t *cli, const char *keys_json) {
  if (!cli || !keys_json) return;
  snprintf(cli->last_sent_checkin_keys_json, sizeof(cli->last_sent_checkin_keys_json), "%s", keys_json);
  cli->has_last_sent_checkin_keys = 1;
}

int eh_checkin_keys_drifted(const eh_client_t *cli) {
  char current[EH_MAX_CHECKIN_KEYS + 2];

  if (!cli || !cli->has_last_sent_checkin_keys) return 0;
  if (eh_checkin_build_keys_json((eh_client_t *)cli, current, sizeof(current)) != EH_OK) return -1;
  return strcmp(current, cli->last_sent_checkin_keys_json) != 0 ? 1 : 0;
}

static eh_status_t build_checkin_url(eh_client_t *cli, char *url, size_t cap) {
  int n;

  if (cli->cfg.tenant_slug[0]) {
    n = snprintf(
      url, cap, "%s/api/v1/mdm/public/%s/device/checkin",
      cli->cfg.base_url, cli->cfg.tenant_slug
    );
  } else {
    n = snprintf(url, cap, "%s/api/v1/mdm/public/device/checkin", cli->cfg.base_url);
  }
  if (n <= 0 || (size_t)n >= cap) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static eh_status_t build_checkin_json(
  const eh_client_t *cli,
  char *body,
  size_t cap,
  size_t *out_len
) {
  char keys_json[EH_MAX_CHECKIN_KEYS + 2];
  eh_status_t rc;
  size_t len = 0;

  rc = eh_checkin_build_keys_json((eh_client_t *)cli, keys_json, sizeof(keys_json));
  if (rc != EH_OK) return rc;

  if (!eh_json_append(body, cap, &len, "{\"deviceUid\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(body, cap, &len, cli->cfg.device_uid)) return EH_ERR_BUFFER_TOO_SMALL;
  if (has_full_checkin_params(cli)) {
    if (!eh_json_append(body, cap, &len, "\",\"groupKey\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_escape_append(body, cap, &len, cli->cfg.group_key)) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_append(body, cap, &len, "\",\"productCode\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_escape_append(body, cap, &len, cli->cfg.product_code)) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_append(body, cap, &len, "\",\"keys\":")) return EH_ERR_BUFFER_TOO_SMALL;
  } else {
    if (!eh_json_append(body, cap, &len, "\",\"keys\":")) return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!eh_json_append(body, cap, &len, keys_json)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(body, cap, &len, "}")) return EH_ERR_BUFFER_TOO_SMALL;
  *out_len = len;
  return EH_OK;
}

static void dispatch_checkin_downloads(eh_client_t *cli, const char *resp) {
  char *map_json = NULL;
  char *entry_json = NULL;
  const char *cursor;
  char key[EH_MAX_CHECKIN_ASSET_KEY];
  char version[EH_MAX_CHECKIN_VERSION];
  char url[EH_MAX_DOWNLOAD_URL];
  char checksum[EH_MAX_CHECKSUM_SHA256];
  int entry_rc = 0;

  if (!cli || !cli->cb.on_checkin_download || !resp) return;

  map_json = (char *)malloc(EH_MAX_PAYLOAD);
  entry_json = (char *)malloc(EH_MAX_CHECKIN_CONFIG_ENTRY);
  if (!map_json || !entry_json) {
    eh_logf(cli, EH_LOG_WARN, "checkin_download_oom");
    goto cleanup;
  }

  if (!eh_find_json_raw_value(resp, "downloads", map_json, EH_MAX_PAYLOAD)) goto cleanup;
  if (map_json[0] != '{') goto cleanup;

  cursor = map_json + 1;
  while ((entry_rc = eh_json_next_map_entry(&cursor, key, sizeof(key), entry_json, EH_MAX_CHECKIN_CONFIG_ENTRY)) > 0) {
    version[0] = '\0';
    url[0] = '\0';
    checksum[0] = '\0';
    if (!eh_find_json_string_value(entry_json, "version", version, sizeof(version))) continue;
    if (!eh_find_json_string_value(entry_json, "downloadUrl", url, sizeof(url))) continue;
    if (!eh_find_json_string_value(entry_json, "checksumSha256", checksum, sizeof(checksum))) {
      checksum[0] = '\0';
    }
    cli->cb.on_checkin_download(cli->cb.user_ctx, key, version, url, checksum);
    eh_logf(cli, EH_LOG_INFO, "checkin_download_offered");
  }
  if (entry_rc < 0) {
    eh_logf(cli, EH_LOG_WARN, "checkin_download_entry_parse_failed");
  }

cleanup:
  free(map_json);
  free(entry_json);
}

static void dispatch_checkin_configurations(eh_client_t *cli, const char *resp) {
  char *map_json = NULL;
  char *entry_json = NULL;
  char *payload = NULL;
  const char *cursor;
  char key[EH_MAX_CHECKIN_ASSET_KEY];
  char version[EH_MAX_CHECKIN_VERSION];
  char source[EH_MAX_CHECKIN_CONFIG_SOURCE];
  int entry_rc = 0;

  if (!cli || !cli->cb.on_checkin_configuration || !resp) return;

  map_json = (char *)malloc(EH_MAX_PAYLOAD);
  entry_json = (char *)malloc(EH_MAX_CHECKIN_CONFIG_ENTRY);
  payload = (char *)malloc(EH_MAX_CHECKIN_CONFIG_PAYLOAD);
  if (!map_json || !entry_json || !payload) {
    eh_logf(cli, EH_LOG_WARN, "checkin_configuration_oom");
    goto cleanup;
  }

  if (!eh_find_json_raw_value(resp, "configurations", map_json, EH_MAX_PAYLOAD)) goto cleanup;
  if (map_json[0] != '{') goto cleanup;

  cursor = map_json + 1;
  while ((entry_rc = eh_json_next_map_entry(&cursor, key, sizeof(key), entry_json, EH_MAX_CHECKIN_CONFIG_ENTRY)) > 0) {
    version[0] = '\0';
    source[0] = '\0';
    payload[0] = '\0';
    if (!eh_find_json_string_value(entry_json, "version", version, sizeof(version))) continue;
    if (!eh_find_json_string_value(entry_json, "source", source, sizeof(source))) {
      source[0] = '\0';
    }
    if (!eh_find_json_raw_value(entry_json, "payload", payload, EH_MAX_CHECKIN_CONFIG_PAYLOAD)) continue;
    cli->cb.on_checkin_configuration(cli->cb.user_ctx, key, version, source, payload);
    eh_logf(cli, EH_LOG_INFO, "checkin_configuration_offered");
  }
  if (entry_rc < 0) {
    eh_logf(cli, EH_LOG_WARN, "checkin_configuration_entry_parse_failed");
  }

cleanup:
  free(map_json);
  free(entry_json);
  free(payload);
}

static eh_status_t parse_checkin_response(eh_client_t *cli, const char *resp) {
  char status[32];

  status[0] = '\0';
  if (eh_find_json_string_value(resp, "provisioningStatus", status, sizeof(status))) {
    if (strcmp(status, "active") == 0) {
      cli->provisioning = EH_PROV_ACTIVE;
    } else if (strcmp(status, "pending") == 0) {
      cli->provisioning = EH_PROV_PENDING;
    }
  }
  dispatch_checkin_downloads(cli, resp);
  dispatch_checkin_configurations(cli, resp);
  return EH_OK;
}

eh_status_t eh_run_checkin(eh_client_t *cli) {
  char url[EH_MAX_URL + 128];
  char keys_sent[EH_MAX_CHECKIN_KEYS + 2];
  char *body = NULL;
  uint8_t *resp = NULL;
  size_t body_len = 0;
  size_t resp_len = 0;
  int http_status = 0;
  uint32_t retry_after_sec = 0;
  eh_status_t rc;
  int http_ok;

  if (!cli || (!cli->cb.http_post && !cli->cb.http_post_ex)) return EH_ERR_INVALID_ARG;
  if (!cli->cfg.enable_checkin) return EH_OK;

  body = (char *)malloc(EH_MAX_PAYLOAD);
  resp = (uint8_t *)malloc(EH_MAX_RESPONSE);
  if (!body || !resp) {
    free(body);
    free(resp);
    eh_logf(cli, EH_LOG_ERROR, "checkin_oom");
    return EH_ERR_IO;
  }

  rc = build_checkin_url(cli, url, sizeof(url));
  if (rc != EH_OK) goto done;

  rc = eh_checkin_build_keys_json(cli, keys_sent, sizeof(keys_sent));
  if (rc != EH_OK) goto done;

  rc = build_checkin_json(cli, body, EH_MAX_PAYLOAD, &body_len);
  if (rc != EH_OK) goto done;

  http_ok = eh_client_http_post_hmac(
      cli,
      url,
      body,
      (cli->has_secret_current && !cli->needs_credential_reprovision) ? 1 : 0,
      (const uint8_t *)body,
      body_len,
      resp,
      EH_MAX_RESPONSE,
      &resp_len,
      &http_status,
      &retry_after_sec);
  if (resp_len >= EH_MAX_RESPONSE) resp_len = EH_MAX_RESPONSE - 1;
  resp[resp_len] = '\0';
  if (resp_len >= EH_MAX_RESPONSE - 1) {
    eh_logf(cli, EH_LOG_WARN, "checkin_response_truncated");
  }
  if (!http_ok || http_status < 200 || http_status > 299) {
    char fail_msg[48];
    uint8_t had_secret = cli->has_secret_current;
    if (http_status == 401 &&
        eh_json_has_token((const char *)resp, "CHECKIN_DEVICE_IDENTITY_NOT_REGISTERED") &&
        retry_after_sec == 0) {
      retry_after_sec = 300;
    }
    eh_client_note_http_retry(cli, http_status, retry_after_sec);
    if (eh_checkin_response_requires_credential_reprovision(http_status, (const char *)resp)) {
      cli->needs_credential_reprovision = 1;
      eh_logf(cli, EH_LOG_WARN, "checkin_credential_reprovision_required");
    }
    if (http_ok && resp_len > 0) {
      eh_status_t issue_rc = eh_apply_identity_credential_issue_from_checkin(cli, (const char *)resp);
      if (issue_rc != EH_OK) eh_logf(cli, EH_LOG_WARN, "identity_credential_issue_failed");
      if (!had_secret && cli->has_secret_current) {
        rc = EH_OK;
        goto done;
      }
    }
    snprintf(fail_msg, sizeof(fail_msg), "checkin_fail http=%d", http_status);
    eh_logf(cli, EH_LOG_WARN, fail_msg);
    rc = EH_ERR_HTTP;
    goto done;
  }
  rc = parse_checkin_response(cli, (const char *)resp);
  if (rc == EH_OK) {
    eh_checkin_store_sent_keys(cli, keys_sent);
    eh_status_t issue_rc = eh_apply_identity_credential_issue_from_checkin(cli, (const char *)resp);
    if (issue_rc != EH_OK) eh_logf(cli, EH_LOG_WARN, "identity_credential_issue_failed");
    eh_logf(cli, EH_LOG_INFO, "checkin_ok");
  }

done:
  free(body);
  free(resp);
  return rc;
}
