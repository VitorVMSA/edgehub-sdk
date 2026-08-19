#ifndef EHC_COMMANDS_H
#define EHC_COMMANDS_H

#include "ehc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ehc_args ehc_args_t;

typedef struct {
  int success;
  char message[EHC_CMD_MESSAGE_MAX];
} ehc_cmd_result_t;

typedef int (*ehc_cmd_handler_t)(const ehc_args_t *args, ehc_cmd_result_t *result, void *ctx);

#define EHC_CMD_DEFER_AFTER_ACK 1u

typedef struct {
  const char *key;
  ehc_cmd_handler_t handler;
  uint32_t flags;
} ehc_command_t;

#define EHC_COMMAND(key, handler) { (key), (handler), 0 }
#define EHC_COMMAND_FLAGS(key, handler, flags) { (key), (handler), (flags) }

int ehc_args_has(const ehc_args_t *args, const char *key);
double ehc_args_num(const ehc_args_t *args, const char *key, double def);
long ehc_args_int(const ehc_args_t *args, const char *key, long def);
int ehc_args_bool(const ehc_args_t *args, const char *key, int def);
const char *ehc_args_str(const ehc_args_t *args, const char *key, const char *def);
const char *ehc_args_raw(const ehc_args_t *args, const char *key);

int ehc_cmd_ok(ehc_cmd_result_t *r, const char *msg);
int ehc_cmd_okf(ehc_cmd_result_t *r, const char *fmt, ...);
int ehc_cmd_fail(ehc_cmd_result_t *r, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
