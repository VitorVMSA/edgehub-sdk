#define EH_LIB_ABI_CHECK_EXPORT 1
#include "../include/edgehub_device.h"

#include <stdio.h>
#include <string.h>
#include "internal/eh_checkin.h"
#include "internal/eh_command_executed.h"
#include "internal/eh_commands.h"
#include "internal/eh_json.h"
#include "internal/eh_log.h"
#include "internal/eh_provision.h"
#include "internal/eh_telemetry.h"
#include "internal/eh_telemetry_queue.h"

static uint64_t clamp_next(uint64_t now_ms, uint32_t delta_ms) {
  return now_ms + (uint64_t)delta_ms;
}

static eh_status_t schedule_backoff(eh_client_t *cli, uint64_t now_ms) {
  uint32_t next;
  uint8_t server_directed = 0;

  if (cli->last_retry_after_sec > 0) {
    char msg[48];
    next = cli->last_retry_after_sec * 1000u;
    snprintf(msg, sizeof(msg), "backoff_retry_after=%u", (unsigned)cli->last_retry_after_sec);
    eh_logf(cli, EH_LOG_WARN, msg);
    cli->last_retry_after_sec = 0;
    server_directed = 1;
  } else {
    next = cli->backoff_current_ms ? cli->backoff_current_ms : cli->cfg.backoff_initial_ms;
    if (next == 0) {
      next = 1000;
    }
    if (next > cli->cfg.backoff_max_ms && cli->cfg.backoff_max_ms > 0) {
      next = cli->cfg.backoff_max_ms;
    }
    if (next < 0x7FFFFFFFu) {
      uint32_t doubled = next * 2u;
      cli->backoff_current_ms = (doubled < next) ? next : doubled;
      if (cli->cfg.backoff_max_ms > 0 && cli->backoff_current_ms > cli->cfg.backoff_max_ms) {
        cli->backoff_current_ms = cli->cfg.backoff_max_ms;
      }
    }
  }

  if (next == 0) {
    next = 1000;
  }
  if (!server_directed && cli->cfg.backoff_max_ms > 0 && next > cli->cfg.backoff_max_ms) {
    next = cli->cfg.backoff_max_ms;
  }
  cli->backoff_until_ms = now_ms + next;
  return EH_OK;
}

static void apply_config_defaults(eh_config_t *cfg) {
  if (!cfg->enable_checkin && !cfg->enable_telemetry && !cfg->enable_commands) {
    cfg->enable_checkin = 1;
    cfg->enable_telemetry = 1;
    cfg->enable_commands = 1;
  }
  if (!cfg->checkin_interval_ms) cfg->checkin_interval_ms = 60000;
  if (!cfg->telemetry_interval_ms) cfg->telemetry_interval_ms = 10000;
  if (!cfg->backoff_initial_ms) cfg->backoff_initial_ms = 2000;
  if (!cfg->backoff_max_ms) cfg->backoff_max_ms = 60000;
  if (!cfg->checkin_on_keys_change) cfg->checkin_on_keys_change = 1;
}

EH_API eh_status_t eh_values_begin(eh_values_writer_t *w, char *buf, size_t cap) {
  if (!w || !buf || cap < 3) return EH_ERR_INVALID_ARG;
  memset(w, 0, sizeof(*w));
  w->buf = buf;
  w->cap = cap;
  w->len = 0;
  w->started = 1;
  w->has_any = 0;
  w->buf[0] = '{';
  w->buf[1] = '\0';
  w->len = 1;
  return EH_OK;
}

static eh_status_t add_kv_prefix(eh_values_writer_t *w, const char *key) {
  size_t len = w->len;
  if (!w || !key) return EH_ERR_INVALID_ARG;
  if (w->has_any) {
    if (!eh_json_append(w->buf, w->cap, &len, ",")) return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!eh_json_append(w->buf, w->cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(w->buf, w->cap, &len, key)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(w->buf, w->cap, &len, "\":")) return EH_ERR_BUFFER_TOO_SMALL;
  w->len = len;
  return EH_OK;
}

EH_API eh_status_t eh_values_add_str(eh_values_writer_t *w, const char *key, const char *value) {
  size_t len;
  eh_status_t rc = add_kv_prefix(w, key);
  if (rc != EH_OK) return rc;
  len = w->len;
  if (!eh_json_append(w->buf, w->cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(w->buf, w->cap, &len, value ? value : "")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(w->buf, w->cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  w->len = len;
  w->has_any = 1;
  return EH_OK;
}

EH_API eh_status_t eh_values_add_num(eh_values_writer_t *w, const char *key, double value) {
  char num[48];
  size_t len;
  eh_status_t rc = add_kv_prefix(w, key);
  if (rc != EH_OK) return rc;
  snprintf(num, sizeof(num), "%.15g", value);
  len = w->len;
  if (!eh_json_append(w->buf, w->cap, &len, num)) return EH_ERR_BUFFER_TOO_SMALL;
  w->len = len;
  w->has_any = 1;
  return EH_OK;
}

EH_API eh_status_t eh_values_add_bool(eh_values_writer_t *w, const char *key, int value) {
  size_t len;
  eh_status_t rc = add_kv_prefix(w, key);
  if (rc != EH_OK) return rc;
  len = w->len;
  if (!eh_json_append(w->buf, w->cap, &len, value ? "true" : "false")) return EH_ERR_BUFFER_TOO_SMALL;
  w->len = len;
  w->has_any = 1;
  return EH_OK;
}

EH_API eh_status_t eh_values_add_raw(eh_values_writer_t *w, const char *key, const char *raw_json) {
  size_t len;
  eh_status_t rc;
  if (!raw_json) return EH_ERR_INVALID_ARG;
  rc = add_kv_prefix(w, key);
  if (rc != EH_OK) return rc;
  len = w->len;
  if (!eh_json_append(w->buf, w->cap, &len, raw_json)) return EH_ERR_BUFFER_TOO_SMALL;
  w->len = len;
  w->has_any = 1;
  return EH_OK;
}

EH_API eh_status_t eh_values_end(eh_values_writer_t *w) {
  size_t len;
  if (!w || !w->started) return EH_ERR_INVALID_ARG;
  len = w->len;
  if (!eh_json_append(w->buf, w->cap, &len, "}")) return EH_ERR_BUFFER_TOO_SMALL;
  w->len = len;
  w->started = 0;
  return EH_OK;
}

EH_API eh_status_t eh_client_init(eh_client_t *cli, const eh_config_t *cfg, const eh_callbacks_t *cb, uint64_t now_ms) {
  if (!cli || !cfg || !cb) return EH_ERR_INVALID_ARG;
  memset(cli, 0, sizeof(*cli));
  memcpy(&cli->cfg, cfg, sizeof(*cfg));
  memcpy(&cli->cb, cb, sizeof(*cb));
  apply_config_defaults(&cli->cfg);

  if (!cli->cfg.base_url[0] || !cli->cfg.device_uid[0]) return EH_ERR_INVALID_ARG;
  if ((!cli->cb.http_post && !cli->cb.http_post_ex) || !cli->cb.telemetry_collect) {
    return EH_ERR_INVALID_ARG;
  }

  cli->initialized = 1;
  cli->provisioning = EH_PROV_UNKNOWN;
  cli->next_checkin_at_ms = now_ms;
  if (cli->cfg.telemetry_manual_only) {
    cli->next_telemetry_at_ms = (uint64_t)-1;
  } else {
    cli->next_telemetry_at_ms = now_ms + cli->cfg.telemetry_interval_ms;
  }
  cli->backoff_current_ms = cli->cfg.backoff_initial_ms;
  snprintf(cli->checkin_keys_json, sizeof(cli->checkin_keys_json), "{}");
  eh_telemetry_queue_load(cli);
  eh_logf(cli, EH_LOG_INFO, "client_initialized");
  return EH_OK;
}

EH_API eh_status_t eh_client_set_group_product(eh_client_t *cli, const char *group_key, const char *product_code) {
  if (!cli) return EH_ERR_INVALID_ARG;
  snprintf(cli->cfg.group_key, sizeof(cli->cfg.group_key), "%s", group_key ? group_key : "");
  snprintf(cli->cfg.product_code, sizeof(cli->cfg.product_code), "%s", product_code ? product_code : "");
  return EH_OK;
}

EH_API eh_status_t eh_client_set_secret(eh_client_t *cli, const char *current_secret, const char *previous_secret) {
  if (!cli) return EH_ERR_INVALID_ARG;
  cli->has_secret_current = 0;
  cli->has_secret_previous = 0;
  if (current_secret && current_secret[0]) {
    snprintf(cli->secret_current, sizeof(cli->secret_current), "%s", current_secret);
    cli->has_secret_current = 1;
  }
  if (previous_secret && previous_secret[0]) {
    snprintf(cli->secret_previous, sizeof(cli->secret_previous), "%s", previous_secret);
    cli->has_secret_previous = 1;
  }
  return EH_OK;
}

EH_API eh_status_t eh_client_set_telemetry_ts(eh_client_t *cli, const char *ts_iso) {
  if (!cli) return EH_ERR_INVALID_ARG;
  snprintf(cli->cfg.telemetry_ts, sizeof(cli->cfg.telemetry_ts), "%s", ts_iso ? ts_iso : "");
  return EH_OK;
}

EH_API eh_status_t eh_client_set_telemetry_interval(eh_client_t *cli, uint32_t interval_ms) {
  if (!cli) return EH_ERR_INVALID_ARG;
  if (!interval_ms) interval_ms = 10000;
  cli->cfg.telemetry_interval_ms = interval_ms;
  return EH_OK;
}

EH_API eh_status_t eh_client_set_telemetry_batch_max(eh_client_t *cli, uint8_t batch_max) {
  if (!cli) return EH_ERR_INVALID_ARG;
  cli->cfg.telemetry_batch_max = batch_max;
  return EH_OK;
}

EH_API eh_status_t eh_client_enqueue_reading(eh_client_t *cli, const char *ts_iso, uint64_t now_ms) {
  if (!cli || !cli->initialized) return EH_ERR_NOT_INITIALIZED;
  if (!cli->cfg.enable_telemetry) return EH_OK;
  return eh_telemetry_collect_and_enqueue(cli, ts_iso, now_ms);
}

EH_API eh_status_t eh_client_flush_telemetry(eh_client_t *cli, uint64_t now_ms) {
  eh_status_t rc;
  if (!cli || !cli->initialized) return EH_ERR_NOT_INITIALIZED;
  if (!cli->cfg.enable_telemetry) return EH_OK;
  rc = eh_flush_telemetry(cli);
  if (rc != EH_OK) {
    schedule_backoff(cli, now_ms);
    return rc;
  }
  rc = eh_process_pending_commands(cli, now_ms);
  if (rc != EH_OK) {
    schedule_backoff(cli, now_ms);
    return rc;
  }
  cli->backoff_current_ms = cli->cfg.backoff_initial_ms;
  if (!cli->cfg.telemetry_manual_only) {
    cli->next_telemetry_at_ms = clamp_next(now_ms, cli->cfg.telemetry_interval_ms);
  }
  return EH_OK;
}

EH_API uint8_t eh_client_telemetry_queue_count(const eh_client_t *cli) {
  return cli ? cli->telemetry_queue_count : 0;
}

EH_API void eh_client_telemetry_queue_clear(eh_client_t *cli) {
  if (!cli) return;
  eh_telemetry_queue_drop_front(cli, cli->telemetry_queue_count);
}

EH_API eh_status_t eh_client_set_checkin_interval(eh_client_t *cli, uint32_t interval_ms) {
  if (!cli) return EH_ERR_INVALID_ARG;
  if (!interval_ms) interval_ms = 60000;
  cli->cfg.checkin_interval_ms = interval_ms;
  return EH_OK;
}

EH_API eh_status_t eh_client_set_checkin_keys_json(eh_client_t *cli, const char *keys_json_object) {
  size_t len;
  if (!cli) return EH_ERR_INVALID_ARG;
  if (!keys_json_object || !keys_json_object[0]) {
    snprintf(cli->checkin_keys_json, sizeof(cli->checkin_keys_json), "{}");
    return EH_OK;
  }
  len = strlen(keys_json_object);
  if (len < 2 || keys_json_object[0] != '{' || keys_json_object[len - 1] != '}') {
    return EH_ERR_INVALID_ARG;
  }
  if (len >= sizeof(cli->checkin_keys_json)) return EH_ERR_BUFFER_TOO_SMALL;
  snprintf(cli->checkin_keys_json, sizeof(cli->checkin_keys_json), "%s", keys_json_object);
  return EH_OK;
}

EH_API eh_provisioning_status_t eh_client_get_provisioning(const eh_client_t *cli) {
  return cli ? cli->provisioning : EH_PROV_UNKNOWN;
}

EH_API void eh_client_get_status(const eh_client_t *cli, eh_client_status_t *out) {
  if (!cli || !out) return;
  memset(out, 0, sizeof(*out));
  out->provisioning = cli->provisioning;
  out->next_checkin_at_ms = cli->next_checkin_at_ms;
  out->next_telemetry_at_ms = cli->next_telemetry_at_ms;
  out->backoff_until_ms = cli->backoff_until_ms;
  out->backoff_current_ms = cli->backoff_current_ms;
  out->pending_command_count = cli->pending_command_count;
  out->initialized = cli->initialized;
}

EH_API eh_status_t eh_client_get_pending_command(const eh_client_t *cli, uint8_t index, eh_pending_command_t *out) {
  if (!cli || !out) return EH_ERR_INVALID_ARG;
  if (index >= cli->pending_command_count) return EH_ERR_INVALID_ARG;
  memcpy(out, &cli->pending_commands[index], sizeof(*out));
  return EH_OK;
}

EH_API eh_status_t eh_client_confirm_command_execution(
  eh_client_t *cli,
  const char *execution_id,
  int success,
  const char *message
) {
  if (!cli || !cli->initialized) return EH_ERR_NOT_INITIALIZED;
  return eh_run_command_executed(cli, execution_id, success, message);
}

EH_API eh_status_t eh_client_force_checkin(eh_client_t *cli, uint64_t now_ms) {
  eh_status_t rc;
  if (!cli || !cli->initialized) return EH_ERR_NOT_INITIALIZED;
  if (!cli->cfg.enable_checkin) return EH_OK;
  rc = eh_run_checkin(cli);
  if (rc != EH_OK) {
    schedule_backoff(cli, now_ms);
    return rc;
  }
  cli->backoff_current_ms = cli->cfg.backoff_initial_ms;
  cli->next_checkin_at_ms = clamp_next(now_ms, cli->cfg.checkin_interval_ms);
  return EH_OK;
}

EH_API eh_status_t eh_client_force_telemetry(eh_client_t *cli, uint64_t now_ms) {
  eh_status_t rc;
  if (!cli || !cli->initialized) return EH_ERR_NOT_INITIALIZED;
  if (!cli->cfg.enable_telemetry) return EH_OK;
  rc = eh_run_telemetry(cli, now_ms);
  if (rc != EH_OK) {
    schedule_backoff(cli, now_ms);
    return rc;
  }
  rc = eh_process_pending_commands(cli, now_ms);
  if (rc != EH_OK) {
    schedule_backoff(cli, now_ms);
    return rc;
  }
  cli->backoff_current_ms = cli->cfg.backoff_initial_ms;
  if (!cli->cfg.telemetry_manual_only) {
    cli->next_telemetry_at_ms = clamp_next(now_ms, cli->cfg.telemetry_interval_ms);
  }
  return EH_OK;
}

EH_API eh_status_t eh_client_poll_downlink(eh_client_t *cli, uint64_t now_ms) {
  eh_status_t rc;
  if (!cli || !cli->initialized) return EH_ERR_NOT_INITIALIZED;
  rc = eh_poll_downlink(cli, now_ms);
  if (rc != EH_OK) return rc;
  return eh_process_pending_commands(cli, now_ms);
}

EH_API eh_status_t eh_client_tick(eh_client_t *cli, uint64_t now_ms) {
  eh_status_t rc = EH_OK;
  eh_status_t step;
  if (!cli || !cli->initialized) return EH_ERR_NOT_INITIALIZED;

  if (cli->credential_expires_at_ms && now_ms >= cli->credential_expires_at_ms) {
    cli->needs_credential_reprovision = 1;
  }

  if (
    cli->cfg.enable_checkin &&
    cli->cfg.checkin_on_keys_change &&
    cli->has_last_sent_checkin_keys
  ) {
    int drift = eh_checkin_keys_drifted(cli);
    if (drift > 0) {
      step = eh_client_force_checkin(cli, now_ms);
      if (rc == EH_OK) rc = step;
    }
  }

  if (cli->backoff_until_ms && now_ms < cli->backoff_until_ms) return rc;
  if (cli->backoff_until_ms && now_ms >= cli->backoff_until_ms) cli->backoff_until_ms = 0;

  if (cli->needs_credential_reprovision && cli->cfg.enable_checkin) {
    step = eh_client_force_checkin(cli, now_ms);
    if (rc == EH_OK) rc = step;
  }

  if (cli->cfg.enable_checkin && now_ms >= cli->next_checkin_at_ms) {
    step = eh_client_force_checkin(cli, now_ms);
    if (rc == EH_OK) rc = step;
  }
  if (
    cli->cfg.enable_telemetry &&
    !cli->cfg.telemetry_manual_only &&
    now_ms >= cli->next_telemetry_at_ms
  ) {
    step = eh_client_force_telemetry(cli, now_ms);
    if (rc == EH_OK) rc = step;
  }
  return rc;
}

EH_API size_t eh_struct_size_config(void) { return sizeof(eh_config_t); }
EH_API size_t eh_struct_size_client(void) { return sizeof(eh_client_t); }
EH_API size_t eh_struct_size_callbacks(void) { return sizeof(eh_callbacks_t); }
EH_API size_t eh_struct_size_status(void) { return sizeof(eh_client_status_t); }

static const char s_eh_lib_version[] =
    EH_LIB_VERSION_STRINGIFY(EH_LIB_VERSION_MAJOR) "."
    EH_LIB_VERSION_STRINGIFY(EH_LIB_VERSION_MINOR) "."
    EH_LIB_VERSION_STRINGIFY(EH_LIB_VERSION_PATCH);

EH_API const char *eh_lib_version_string(void) { return s_eh_lib_version; }

EH_API unsigned eh_lib_abi_version(void) { return EH_LIB_ABI_VERSION; }

EH_API eh_status_t eh_lib_abi_check_impl(
  size_t config_sz,
  size_t client_sz,
  size_t callbacks_sz,
  size_t status_sz,
  unsigned abi
) {
  if (abi != EH_LIB_ABI_VERSION) {
    return EH_ERR_INVALID_ARG;
  }
  if (config_sz != sizeof(eh_config_t)) {
    return EH_ERR_INVALID_ARG;
  }
  if (client_sz != sizeof(eh_client_t)) {
    return EH_ERR_INVALID_ARG;
  }
  if (callbacks_sz != sizeof(eh_callbacks_t)) {
    return EH_ERR_INVALID_ARG;
  }
  if (status_sz != sizeof(eh_client_status_t)) {
    return EH_ERR_INVALID_ARG;
  }
  return EH_OK;
}

EH_API eh_status_t eh_lib_abi_check(void) {
  return eh_lib_abi_check_impl(
    sizeof(eh_config_t),
    sizeof(eh_client_t),
    sizeof(eh_callbacks_t),
    sizeof(eh_client_status_t),
    EH_LIB_ABI_VERSION
  );
}
