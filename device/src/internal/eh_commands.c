#include "eh_commands.h"

#include <stdio.h>
#include <string.h>
#include "eh_command_executed.h"
#include "eh_json.h"
#include "eh_log.h"

static const char EH_SYS_CMD_FORCE_CHECKIN[] = "device.force_checkin";

static void fill_command_message(
  eh_client_t *cli,
  const char *execution_id,
  const char *command_key,
  int success,
  char *message,
  size_t message_cap
) {
  message[0] = '\0';
  if (!cli->cb.on_command_message || message_cap == 0) return;
  cli->cb.on_command_message(cli->cb.user_ctx, execution_id, command_key, success, message, message_cap);
}

static eh_status_t confirm_command_if_enabled(
  eh_client_t *cli,
  const char *execution_id,
  const char *command_key,
  int success
) {
  char message[EH_MAX_DEVICE_MESSAGE + 1];
  eh_status_t rc;

  if (!cli->cfg.enable_auto_confirm_commands) return EH_OK;
  fill_command_message(cli, execution_id, command_key, success, message, sizeof(message));
  rc = eh_run_command_executed(cli, execution_id, success, message[0] ? message : NULL);
  if (rc != EH_OK) {
    eh_logf(cli, EH_LOG_WARN, "command_confirm_failed");
  }
  return rc;
}

static int is_force_checkin_command(const char *command_key) {
  return command_key && strcmp(command_key, EH_SYS_CMD_FORCE_CHECKIN) == 0;
}

eh_status_t eh_process_pending_commands(eh_client_t *cli, uint64_t now_ms) {
  uint8_t i;
  if (!cli) return EH_ERR_INVALID_ARG;
  if (!cli->cfg.enable_commands) return EH_OK;

  for (i = 0; i < cli->pending_command_count; i++) {
    eh_pending_command_t *cmd = &cli->pending_commands[i];
    int ok = 0;

    if (is_force_checkin_command(cmd->command_key)) {
      /* Built-in EdgeHub system command — do not delegate to on_command. */
      ok = (eh_client_force_checkin(cli, now_ms) == EH_OK) ? 1 : 0;
    } else if (cli->cb.on_command) {
      ok = cli->cb.on_command(cli->cb.user_ctx, cmd->execution_id, cmd->command_key, cmd->payload_json);
    }

    if (ok) {
      eh_logf(cli, EH_LOG_INFO, "command_executed");
    } else if (cli->cfg.enable_auto_confirm_commands) {
      eh_logf(cli, EH_LOG_WARN, "command_execution_failed");
    } else {
      eh_logf(cli, EH_LOG_DEBUG, "command_pending_confirmation");
    }
    confirm_command_if_enabled(cli, cmd->execution_id, cmd->command_key, ok);
  }
  cli->pending_command_count = 0;
  return EH_OK;
}

static eh_status_t parse_command_object(eh_client_t *cli, const char *obj_json) {
  char exec_id[EH_MAX_EXEC_ID];
  char cmd_key[EH_MAX_COMMAND_KEY];
  char payload[EH_MAX_COMMAND_PAYLOAD];
  eh_pending_command_t *slot;

  if (cli->pending_command_count >= EH_MAX_COMMANDS) return EH_OK;
  if (!eh_find_json_string_value(obj_json, "executionId", exec_id, sizeof(exec_id))) return EH_OK;
  cmd_key[0] = '\0';
  (void)eh_find_json_string_value(obj_json, "commandKey", cmd_key, sizeof(cmd_key));

  payload[0] = '\0';
  if (!eh_find_json_raw_value(obj_json, "payload", payload, sizeof(payload))) {
    strncpy(payload, "{}", sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';
  }

  slot = &cli->pending_commands[cli->pending_command_count];
  strncpy(slot->execution_id, exec_id, EH_MAX_EXEC_ID - 1);
  slot->execution_id[EH_MAX_EXEC_ID - 1] = '\0';
  strncpy(slot->command_key, cmd_key, EH_MAX_COMMAND_KEY - 1);
  slot->command_key[EH_MAX_COMMAND_KEY - 1] = '\0';
  strncpy(slot->payload_json, payload, EH_MAX_COMMAND_PAYLOAD - 1);
  slot->payload_json[EH_MAX_COMMAND_PAYLOAD - 1] = '\0';
  cli->pending_command_count++;
  {
    char msg[64];
    char short_key[48];
    size_t n = 0;
    while (cmd_key[n] && n < sizeof(short_key) - 1) {
      short_key[n] = cmd_key[n];
      n++;
    }
    short_key[n] = '\0';
    snprintf(msg, sizeof(msg), "pending_cmd=%s", short_key[0] ? short_key : "?");
    eh_logf(cli, EH_LOG_INFO, msg);
  }
  return EH_OK;
}

eh_status_t eh_parse_pending_commands(eh_client_t *cli, const char *json) {
  const char *array_start;
  const char *cursor;
  const char *obj_start;
  const char *obj_end;

  if (!cli || !json) return EH_ERR_INVALID_ARG;
  if (!cli->cfg.enable_commands) return EH_OK;

  cli->pending_command_count = 0;
  array_start = eh_json_find_pending_commands_array(json);
  if (!array_start) return EH_OK;

  cursor = array_start + 1;
  while (cli->pending_command_count < EH_MAX_COMMANDS) {
    obj_start = eh_json_next_object(cursor, &obj_end);
    if (!obj_start) break;
    parse_command_object(cli, obj_start);
    cursor = obj_end;
    while (*cursor && (*cursor == ',' || *cursor == ' ' || *cursor == '\n' || *cursor == '\r')) cursor++;
    if (*cursor == ']') break;
  }
  if (cli->pending_command_count >= EH_MAX_COMMANDS) {
    while (*cursor && (*cursor == ',' || *cursor == ' ' || *cursor == '\n' || *cursor == '\r')) cursor++;
    if (*cursor && *cursor != ']') {
      eh_logf(cli, EH_LOG_WARN, "pending_commands_truncated");
    }
  }
  return EH_OK;
}
