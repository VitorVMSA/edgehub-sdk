#include "ehc_user.h"

static int cmd_set_speed(const ehc_args_t *a, ehc_cmd_result_t *r, void *ctx)
{
  long rpm = ehc_args_int(a, "rpm", 0);
  (void)ctx;
  if (rpm < 0 || rpm > 3000) {
    return ehc_cmd_fail(r, "rpm fora do intervalo");
  }
  return ehc_cmd_okf(r, "rpm=%ld", rpm);
}

static int cmd_reboot(const ehc_args_t *a, ehc_cmd_result_t *r, void *ctx)
{
  (void)a;
  (void)ctx;
  return ehc_cmd_ok(r, "rebooting");
}

const ehc_command_t ehc_user_commands[] = {
  EHC_COMMAND("motor.set_speed", cmd_set_speed),
  EHC_COMMAND_FLAGS("device.reboot", cmd_reboot, EHC_CMD_DEFER_AFTER_ACK),
};

const size_t ehc_user_commands_count = sizeof(ehc_user_commands) / sizeof(ehc_user_commands[0]);
