#include "ehc_internal.h"
#include <limits.h>

int ehc_cmd_ok(ehc_cmd_result_t *r, const char *msg)
{
  if (r) {
    r->success = 1;
    strncpy(r->message, msg ? msg : "", sizeof(r->message) - 1);
    r->message[sizeof(r->message) - 1] = '\0';
  }
  return 1;
}

int ehc_cmd_okf(ehc_cmd_result_t *r, const char *fmt, ...)
{
  va_list ap;
  if (r) {
    r->success = 1;
    va_start(ap, fmt);
    vsnprintf(r->message, sizeof(r->message), fmt ? fmt : "", ap);
    va_end(ap);
  }
  return 1;
}

int ehc_cmd_fail(ehc_cmd_result_t *r, const char *msg)
{
  if (r) {
    r->success = 0;
    strncpy(r->message, msg ? msg : "failed", sizeof(r->message) - 1);
    r->message[sizeof(r->message) - 1] = '\0';
  }
  return 0;
}

int ehc_commands_validate_table(void)
{
  size_t i;
  for (i = 0; i < g_ehc.user.command_count; i++) {
    const ehc_command_t *c = &g_ehc.user.commands[i];
    if (!c->key || !c->key[0] || !c->handler) {
      ehc_logf(EHC_LOG_ERROR, "command %u missing key/handler", (unsigned)i);
      return -1;
    }
    if (strcmp(c->key, "device.force_checkin") == 0) {
      ehc_logf(EHC_LOG_ERROR, "do not register device.force_checkin; it is built into the device SDK");
      return -1;
    }
  }
  return 0;
}

static const ehc_command_t *find_cmd(const char *key)
{
  size_t i;
  for (i = 0; i < g_ehc.user.command_count; i++) {
    if (strcmp(g_ehc.user.commands[i].key, key) == 0) {
      return &g_ehc.user.commands[i];
    }
  }
  return NULL;
}

int ehc_on_command(const char *execution_id, const char *command_key, const char *payload_json)
{
  const ehc_command_t *cmd;
  ehc_args_t args;
  ehc_cmd_result_t result;
  g_ehc.last_cmd_ok = 0;
  g_ehc.last_cmd_msg[0] = '\0';
  if (!command_key) {
    return 0;
  }
  cmd = find_cmd(command_key);
  memset(&result, 0, sizeof(result));
  if (!cmd) {
    ehc_cmd_fail(&result, "unknown command");
    g_ehc.last_cmd_ok = 0;
    strncpy(g_ehc.last_cmd_msg, result.message, sizeof(g_ehc.last_cmd_msg) - 1);
    return 0;
  }
  (void)ehc_json_parse_args(payload_json, &args);
  if (cmd->flags & EHC_CMD_DEFER_AFTER_ACK) {
    g_ehc.deferred.cmd = cmd;
    strncpy(g_ehc.deferred.payload, payload_json ? payload_json : "{}", sizeof(g_ehc.deferred.payload) - 1);
    strncpy(g_ehc.deferred.execution_id, execution_id ? execution_id : "", sizeof(g_ehc.deferred.execution_id) - 1);
    g_ehc.has_deferred = 1;
    ehc_cmd_ok(&result, "ack");
    g_ehc.last_cmd_ok = 1;
    strncpy(g_ehc.last_cmd_msg, result.message, sizeof(g_ehc.last_cmd_msg) - 1);
    return 1;
  }
  result.success = 1;
  (void)cmd->handler(&args, &result, g_ehc.user.user_ctx);
  g_ehc.last_cmd_ok = result.success ? 1 : 0;
  strncpy(g_ehc.last_cmd_msg, result.message, sizeof(g_ehc.last_cmd_msg) - 1);
  return result.success ? 1 : 0;
}

int ehc_on_command_message(
  const char *execution_id, const char *command_key, int success, char *message_out, size_t message_cap)
{
  size_t n;
  (void)execution_id;
  (void)command_key;
  (void)success;
  if (!message_out || message_cap == 0) {
    return 0;
  }
  strncpy(message_out, g_ehc.last_cmd_msg, message_cap - 1);
  message_out[message_cap - 1] = '\0';
  n = strlen(message_out);
  return n > (size_t)INT_MAX ? INT_MAX : (int)n;
}

void ehc_run_deferred(void)
{
  ehc_args_t args;
  ehc_cmd_result_t result;
  if (!g_ehc.has_deferred || !g_ehc.deferred.cmd) {
    return;
  }
  g_ehc.has_deferred = 0;
  memset(&result, 0, sizeof(result));
  (void)ehc_json_parse_args(g_ehc.deferred.payload, &args);
  (void)g_ehc.deferred.cmd->handler(&args, &result, g_ehc.user.user_ctx);
}
