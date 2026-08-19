#ifndef EHC_INTERNAL_H
#define EHC_INTERNAL_H

#include "edgehub_client/ehc.h"
#include "edgehub_device.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define EHC_STORE_SECRET "ehc.hmac.secret"
#define EHC_STORE_SECRET_PREV "ehc.hmac.secret.prev"
#define EHC_ASSET_PREFIX "ehc.asset."

typedef struct {
  uint8_t filled;
  uint8_t str_slot;
  double num;
  int boolean;
  uint32_t name_hash;
} ehc_metric_state_t;

typedef struct {
  const char *key;
  const char *raw;
  size_t raw_len;
  int kind; /* 0 str, 1 num, 2 bool, 3 other */
  char decoded[96];
} ehc_arg_entry_t;

struct ehc_args {
  ehc_arg_entry_t entries[EHC_ARGS_MAX_KEYS];
  int count;
  const char *payload;
};

typedef struct {
  const ehc_command_t *cmd;
  char payload[EH_MAX_COMMAND_PAYLOAD];
  char execution_id[EH_MAX_EXEC_ID];
} ehc_deferred_cmd_t;

typedef struct {
  uint8_t used;
  eh_client_t eh;
  ehc_port_t port;
  ehc_user_t user;
  ehc_readiness_t readiness;
  char not_ready[160];
  uint64_t last_not_ready_log_ms;
  uint8_t eh_inited;
  char last_secret[EH_MAX_SECRET];
  ehc_metric_state_t metrics[EHC_MAX_METRICS];
  char str_pool[EHC_TEL_STR_SLOTS][EHC_TEL_STR_MAX];
  uint8_t str_used[EHC_TEL_STR_SLOTS];
  char scratch[EHC_MAX_NESTING_DEPTH][EHC_OBJECT_SCRATCH_BYTES];
  ehc_deferred_cmd_t deferred;
  uint8_t has_deferred;
  char last_cmd_msg[EHC_CMD_MESSAGE_MAX];
  int last_cmd_ok;
} ehc_ctx_t;

extern ehc_ctx_t g_ehc;

void ehc_lock(void);
void ehc_unlock(void);
void ehc_logf(ehc_log_level_t lvl, const char *fmt, ...);
uint64_t ehc_now(void);
int ehc_storage_get(const char *key, char *out, size_t cap);
int ehc_storage_put(const char *key, const char *value);
int ehc_resolve_ident(const ehc_identity_source_t *src, char *out, size_t cap);
void ehc_refresh_readiness(void);
ehc_status_t ehc_ensure_eh_client(void);
void ehc_persist_secret_if_changed(void);
void ehc_load_secret(void);

int ehc_telemetry_validate_table(void);
int ehc_telemetry_collect(eh_values_writer_t *writer);
int ehc_metric_index(const char *parent, const char *name);
int ehc_metric_index_at(const char *path);

int ehc_checkin_validate_table(void);
int ehc_checkin_required_ready(void);
int ehc_checkin_keys_collect(eh_values_writer_t *writer);
void ehc_on_configuration(const char *asset_key, const char *version, const char *source, const char *payload);
int ehc_asset_version(const char *asset_key, char *out, size_t cap);

int ehc_commands_validate_table(void);
int ehc_on_command(const char *execution_id, const char *command_key, const char *payload_json);
int ehc_on_command_message(const char *execution_id, const char *command_key, int success, char *message_out, size_t message_cap);
void ehc_run_deferred(void);
int ehc_json_parse_args(const char *payload, ehc_args_t *out);

int ehc_bridge_http(
  void *user_ctx, const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
  uint32_t *retry_after_sec);
int ehc_bridge_store_read(void *user_ctx, const char *key, uint8_t *buf, size_t cap, size_t *len);
int ehc_bridge_store_write(void *user_ctx, const char *key, const uint8_t *buf, size_t len);
int ehc_bridge_telemetry_collect(void *user_ctx, eh_values_writer_t *writer);
int ehc_bridge_checkin_keys(void *user_ctx, eh_values_writer_t *writer);
int ehc_bridge_on_command(void *user_ctx, const char *execution_id, const char *command_key, const char *payload_json);
int ehc_bridge_on_command_message(void *user_ctx, const char *execution_id, const char *command_key, int success, char *message_out, size_t message_cap);
void ehc_bridge_on_download(void *user_ctx, const char *asset_key, const char *version, const char *url, const char *sum);
void ehc_bridge_on_configuration(void *user_ctx, const char *asset_key, const char *version, const char *source, const char *payload);
void ehc_bridge_log(void *user_ctx, eh_log_level_t level, const char *msg);

#endif
