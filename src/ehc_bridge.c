#include "ehc_internal.h"

int ehc_bridge_http(
  void *user_ctx, const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
  uint32_t *retry_after_sec)
{
  (void)user_ctx;
  if (!g_ehc.port.http_post) {
    return 0;
  }
  return g_ehc.port.http_post(
    g_ehc.port.ctx, url, headers, body, body_len, resp, resp_cap, resp_len, http_status, retry_after_sec);
}

int ehc_bridge_store_read(void *user_ctx, const char *key, uint8_t *buf, size_t cap, size_t *len)
{
  (void)user_ctx;
  if (!g_ehc.port.storage_read) {
    return 0;
  }
  return g_ehc.port.storage_read(g_ehc.port.ctx, key, buf, cap, len);
}

int ehc_bridge_store_write(void *user_ctx, const char *key, const uint8_t *buf, size_t len)
{
  (void)user_ctx;
  if (!g_ehc.port.storage_write) {
    return 0;
  }
  return g_ehc.port.storage_write(g_ehc.port.ctx, key, buf, len);
}

int ehc_bridge_telemetry_collect(void *user_ctx, eh_values_writer_t *writer)
{
  (void)user_ctx;
  return ehc_telemetry_collect(writer);
}

int ehc_bridge_checkin_keys(void *user_ctx, eh_values_writer_t *writer)
{
  (void)user_ctx;
  return ehc_checkin_keys_collect(writer);
}

int ehc_bridge_on_command(void *user_ctx, const char *execution_id, const char *command_key, const char *payload_json)
{
  (void)user_ctx;
  return ehc_on_command(execution_id, command_key, payload_json);
}

int ehc_bridge_on_command_message(
  void *user_ctx, const char *execution_id, const char *command_key, int success, char *message_out, size_t message_cap)
{
  (void)user_ctx;
  return ehc_on_command_message(execution_id, command_key, success, message_out, message_cap);
}

void ehc_bridge_on_download(void *user_ctx, const char *asset_key, const char *version, const char *url, const char *sum)
{
  (void)user_ctx;
  ehc_logf(EHC_LOG_INFO, "download offered %s v%s", asset_key ? asset_key : "", version ? version : "");
  (void)url;
  (void)sum;
}

void ehc_bridge_on_configuration(
  void *user_ctx, const char *asset_key, const char *version, const char *source, const char *payload)
{
  (void)user_ctx;
  ehc_on_configuration(asset_key, version, source, payload);
}

void ehc_bridge_log(void *user_ctx, eh_log_level_t level, const char *msg)
{
  ehc_log_level_t lvl = EHC_LOG_INFO;
  (void)user_ctx;
  if (level == EH_LOG_ERROR) {
    lvl = EHC_LOG_ERROR;
  } else if (level == EH_LOG_WARN) {
    lvl = EHC_LOG_WARN;
  } else if (level == EH_LOG_DEBUG) {
    lvl = EHC_LOG_DEBUG;
  }
  if (g_ehc.port.log) {
    g_ehc.port.log(g_ehc.port.ctx, lvl, msg ? msg : "");
  }
}
