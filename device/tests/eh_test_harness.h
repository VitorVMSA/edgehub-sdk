#ifndef EH_TEST_HARNESS_H
#define EH_TEST_HARNESS_H

#include "edgehub_device.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define EH_TEST_MAX_CAPTURES 16
#define EH_TEST_MAX_BODY 8192
#define EH_TEST_MAX_HEADERS 768
#define EH_TEST_MAX_URL 384
#define EH_TEST_MAX_STORE 8
#define EH_TEST_STORE_BLOB 16384

typedef struct {
  char url[EH_TEST_MAX_URL];
  char headers[EH_TEST_MAX_HEADERS];
  char body[EH_TEST_MAX_BODY];
  size_t body_len;
} eh_test_http_capture_t;

typedef struct {
  char match[96];
  int http_status;
  char body[2048];
  uint32_t retry_after_sec;
  int fail_transport;
} eh_test_http_reply_t;

typedef struct {
  char key[64];
  uint8_t blob[EH_TEST_STORE_BLOB];
  size_t len;
} eh_test_store_slot_t;

typedef struct {
  eh_test_http_capture_t captures[EH_TEST_MAX_CAPTURES];
  int capture_count;
  eh_test_http_reply_t replies[12];
  int reply_count;
  int default_status;
  char default_body[2048];
  int force_offline;
  int store_fail_write;
  uint32_t last_retry_after_sec;
  eh_test_store_slot_t store[EH_TEST_MAX_STORE];
  int store_count;
  int on_command_calls;
  char last_command_key[EH_MAX_COMMAND_KEY];
  char last_command_payload[EH_MAX_COMMAND_PAYLOAD];
  int collect_calls;
  double collect_num;
  const char *collect_str_key;
  const char *collect_str_val;
  int checkin_keys_calls;
  const char *checkin_key_name;
  const char *checkin_key_value;
  int download_calls;
  int config_calls;
  char last_asset_key[128];
  char last_checksum[80];
  char last_source[64];
} eh_test_ctx_t;

static void eh_test_ctx_reset(eh_test_ctx_t *ctx)
{
  memset(ctx, 0, sizeof(*ctx));
  ctx->default_status = 200;
  snprintf(ctx->default_body, sizeof(ctx->default_body), "{}");
}

static void eh_test_push_reply(eh_test_ctx_t *ctx, const char *match, int status, const char *body)
{
  eh_test_http_reply_t *r;
  if (ctx->reply_count >= 12) {
    return;
  }
  r = &ctx->replies[ctx->reply_count++];
  memset(r, 0, sizeof(*r));
  if (match) {
    strncpy(r->match, match, sizeof(r->match) - 1);
  }
  r->http_status = status;
  strncpy(r->body, body ? body : "", sizeof(r->body) - 1);
}

static int eh_test_take_reply(eh_test_ctx_t *ctx, const char *url, eh_test_http_reply_t *out)
{
  int i;
  for (i = 0; i < ctx->reply_count; i++) {
    if (!ctx->replies[i].match[0] || (url && strstr(url, ctx->replies[i].match))) {
      if (out) {
        *out = ctx->replies[i];
      }
      if (i + 1 < ctx->reply_count) {
        memmove(
          &ctx->replies[i],
          &ctx->replies[i + 1],
          (size_t)(ctx->reply_count - i - 1) * sizeof(eh_test_http_reply_t));
      }
      ctx->reply_count--;
      return 1;
    }
  }
  return 0;
}

static int eh_test_http_post(
  void *user_ctx, const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  eh_test_http_reply_t reply_copy;
  int have_reply;
  eh_test_http_capture_t *cap;
  const char *out_body;
  int status;
  size_t n;

  ctx->last_retry_after_sec = 0;
  if (ctx->force_offline) {
    return 0;
  }
  if (ctx->capture_count < EH_TEST_MAX_CAPTURES) {
    cap = &ctx->captures[ctx->capture_count++];
    memset(cap, 0, sizeof(*cap));
    if (url) {
      strncpy(cap->url, url, sizeof(cap->url) - 1);
    }
    if (headers) {
      strncpy(cap->headers, headers, sizeof(cap->headers) - 1);
    }
    if (body && body_len < sizeof(cap->body)) {
      memcpy(cap->body, body, body_len);
      cap->body[body_len] = '\0';
      cap->body_len = body_len;
    }
  }

  have_reply = eh_test_take_reply(ctx, url, &reply_copy);
  if (have_reply && reply_copy.fail_transport) {
    ctx->last_retry_after_sec = reply_copy.retry_after_sec;
    return 0;
  }
  status = have_reply ? reply_copy.http_status : ctx->default_status;
  out_body = have_reply ? reply_copy.body : ctx->default_body;
  ctx->last_retry_after_sec = have_reply ? reply_copy.retry_after_sec : 0;
  n = strlen(out_body);
  if (resp && resp_cap > n) {
    memcpy(resp, out_body, n);
    resp[n] = '\0';
  }
  if (resp_len) {
    *resp_len = n;
  }
  if (http_status) {
    *http_status = status;
  }
  return 1;
}

static int eh_test_http_post_ex(
  void *user_ctx, const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
  uint32_t *retry_after_sec)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  int ok = eh_test_http_post(user_ctx, url, headers, body, body_len, resp, resp_cap, resp_len, http_status);
  if (retry_after_sec) {
    *retry_after_sec = ctx->last_retry_after_sec;
  }
  return ok;
}

static int eh_test_store_read(void *user_ctx, const char *key, uint8_t *buf, size_t cap, size_t *len)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  int i;
  for (i = 0; i < ctx->store_count; i++) {
    if (strcmp(ctx->store[i].key, key) == 0) {
      size_t n = ctx->store[i].len;
      if (n > cap) {
        n = cap;
      }
      memcpy(buf, ctx->store[i].blob, n);
      if (len) {
        *len = n;
      }
      return 1;
    }
  }
  return 0;
}

static int eh_test_store_write(void *user_ctx, const char *key, const uint8_t *buf, size_t len)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  int i;
  eh_test_store_slot_t *slot = NULL;
  if (ctx->store_fail_write) {
    return 0;
  }
  for (i = 0; i < ctx->store_count; i++) {
    if (strcmp(ctx->store[i].key, key) == 0) {
      slot = &ctx->store[i];
      break;
    }
  }
  if (!slot) {
    if (ctx->store_count >= EH_TEST_MAX_STORE) {
      return 0;
    }
    slot = &ctx->store[ctx->store_count++];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->key, key, sizeof(slot->key) - 1);
  }
  if (len > sizeof(slot->blob)) {
    return 0;
  }
  memcpy(slot->blob, buf, len);
  slot->len = len;
  return 1;
}

static int eh_test_telemetry_collect(void *user_ctx, eh_values_writer_t *writer)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  ctx->collect_calls++;
  if (ctx->collect_str_key) {
    if (eh_values_add_str(writer, ctx->collect_str_key, ctx->collect_str_val ? ctx->collect_str_val : "") != EH_OK) {
      return 0;
    }
  } else if (eh_values_add_num(writer, "temp", ctx->collect_num != 0.0 ? ctx->collect_num : 22.5) != EH_OK) {
    return 0;
  }
  return 1;
}

static int eh_test_checkin_keys_collect(void *user_ctx, eh_values_writer_t *writer)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  ctx->checkin_keys_calls++;
  if (ctx->checkin_key_name) {
    if (eh_values_add_str(writer, ctx->checkin_key_name, ctx->checkin_key_value ? ctx->checkin_key_value : "") != EH_OK) {
      return 0;
    }
  }
  return 1;
}

static void eh_test_on_download(
  void *user_ctx, const char *asset_key, const char *version, const char *url, const char *sum)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  (void)version;
  (void)url;
  ctx->download_calls++;
  if (asset_key) {
    strncpy(ctx->last_asset_key, asset_key, sizeof(ctx->last_asset_key) - 1);
  }
  strncpy(ctx->last_checksum, sum ? sum : "", sizeof(ctx->last_checksum) - 1);
}

static void eh_test_on_configuration(
  void *user_ctx, const char *asset_key, const char *version, const char *source, const char *payload)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  (void)version;
  (void)payload;
  ctx->config_calls++;
  if (asset_key) {
    strncpy(ctx->last_asset_key, asset_key, sizeof(ctx->last_asset_key) - 1);
  }
  strncpy(ctx->last_source, source ? source : "", sizeof(ctx->last_source) - 1);
}

static int eh_test_on_command(void *user_ctx, const char *execution_id, const char *command_key, const char *payload_json)
{
  eh_test_ctx_t *ctx = (eh_test_ctx_t *)user_ctx;
  (void)execution_id;
  ctx->on_command_calls++;
  if (command_key) {
    strncpy(ctx->last_command_key, command_key, sizeof(ctx->last_command_key) - 1);
  }
  if (payload_json) {
    strncpy(ctx->last_command_payload, payload_json, sizeof(ctx->last_command_payload) - 1);
  }
  return 1;
}

static void eh_test_fill_cfg(eh_config_t *cfg)
{
  memset(cfg, 0, sizeof(*cfg));
  strncpy(cfg->base_url, "https://example.edgehub.test", sizeof(cfg->base_url) - 1);
  strncpy(cfg->device_uid, "dev-1", sizeof(cfg->device_uid) - 1);
  cfg->enable_checkin = 1;
  cfg->enable_telemetry = 1;
  cfg->enable_commands = 1;
  cfg->enable_auto_confirm_commands = 1;
  cfg->telemetry_manual_only = 1;
  cfg->telemetry_queue_persist = 1;
}

static const char *eh_test_header_sig(const char *headers)
{
  static char hex[65];
  const char *p;
  size_t i;
  if (!headers) {
    return "";
  }
  p = strstr(headers, "x-edgehub-device-signature:");
  if (!p) {
    return "";
  }
  p += strlen("x-edgehub-device-signature:");
  while (*p == ' ') {
    p++;
  }
  for (i = 0; i < 64 && p[i] && p[i] != '\r' && p[i] != '\n'; i++) {
    hex[i] = p[i];
  }
  hex[i] = '\0';
  return hex;
}

#endif
