#include "ehc_test_support.h"
#include "ehc_internal.h"

static int s_speed_rpm;
static int s_reboot_ran;
static int s_ack_before_reboot;

static int cmd_set_speed(const ehc_args_t *a, ehc_cmd_result_t *r, void *ctx)
{
  (void)ctx;
  TEST_ASSERT_TRUE(ehc_args_has(a, "rpm"));
  TEST_ASSERT_FALSE(ehc_args_has(a, "missing"));
  TEST_ASSERT_EQUAL(1500, ehc_args_int(a, "rpm", 0));
  TEST_ASSERT_EQUAL(1500, (long)ehc_args_num(a, "rpm", 0));
  TEST_ASSERT_EQUAL(1, ehc_args_bool(a, "on", 0));
  TEST_ASSERT_EQUAL_STRING("go", ehc_args_str(a, "name", ""));
  TEST_ASSERT_EQUAL(9, ehc_args_int(a, "missing", 9));
  s_speed_rpm = ehc_args_int(a, "rpm", 0);
  return ehc_cmd_okf(r, "rpm=%ld", (long)s_speed_rpm);
}

static int cmd_reboot(const ehc_args_t *a, ehc_cmd_result_t *r, void *ctx)
{
  (void)a;
  (void)ctx;
  s_ack_before_reboot = ehc_host_find_url("/command-executions/") != NULL ? 1 : 0;
  s_reboot_ran = 1;
  return ehc_cmd_ok(r, "bye");
}

static const ehc_command_t s_cmds[] = {
  EHC_COMMAND("motor.set_speed", cmd_set_speed),
  EHC_COMMAND_FLAGS("device.reboot", cmd_reboot, EHC_CMD_DEFER_AFTER_ACK),
};

static const ehc_command_t s_force[] = {
  EHC_COMMAND("device.force_checkin", cmd_reboot),
};

void setUp(void)
{
  ehc_test_begin();
  s_speed_rpm = 0;
  s_reboot_ran = 0;
  s_ack_before_reboot = 0;
}

void tearDown(void)
{
  ehc_shutdown();
}

static void init_cmds(const ehc_command_t *c, size_t n)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.commands = c;
  u.command_count = n;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
}

static void test_dispatch_and_args(void)
{
  int i;
  int executed = 0;
  init_cmds(s_cmds, 2);
  ehc_host_clear_captures();
  ehc_host_push_reply(
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e2\",\"commandKey\":\"motor.set_speed\","
    "\"payload\":{\"rpm\":1500,\"on\":true,\"name\":\"go\"}}]}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  TEST_ASSERT_EQUAL(1500, s_speed_rpm);
  for (i = 0; i < ehc_host_capture_count(); i++) {
    if (strstr(ehc_host_capture(i)->url, "/command-executions/")) {
      executed++;
      TEST_ASSERT(strstr(ehc_host_capture(i)->body, "\"success\":false") == NULL);
      TEST_ASSERT(strstr(ehc_host_capture(i)->body, "rpm=1500") != NULL);
    }
  }
  TEST_ASSERT_EQUAL(1, executed);
}

static void test_unknown_command_fails(void)
{
  int i;
  int executed = 0;
  init_cmds(s_cmds, 2);
  ehc_host_clear_captures();
  ehc_host_push_reply(
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e3\",\"commandKey\":\"no.such\",\"payload\":{}}]}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  for (i = 0; i < ehc_host_capture_count(); i++) {
    if (strstr(ehc_host_capture(i)->url, "/command-executions/")) {
      executed++;
      TEST_ASSERT(strstr(ehc_host_capture(i)->body, "\"success\":false") != NULL);
      TEST_ASSERT(strstr(ehc_host_capture(i)->body, "unknown command") != NULL);
    }
  }
  TEST_ASSERT_EQUAL(1, executed);
}

static void test_defer_after_ack(void)
{
  init_cmds(s_cmds, 2);
  ehc_host_clear_captures();
  ehc_host_push_reply(
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e4\",\"commandKey\":\"device.reboot\",\"payload\":{}}]}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  TEST_ASSERT_EQUAL(1, s_reboot_ran);
  TEST_ASSERT_EQUAL(1, s_ack_before_reboot);
}

static void test_reject_force_checkin_in_table(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.commands = s_force;
  u.command_count = 1;
  TEST_ASSERT_EQUAL(EHC_ECONFIG, ehc_test_init(&u));
}

static void test_args_coercion_and_escape(void)
{
  ehc_args_t args;
  TEST_ASSERT_EQUAL(1, ehc_json_parse_args("{\"n\":\"12\",\"b\":false,\"s\":\"a\\\"b\"}", &args));
  TEST_ASSERT_EQUAL(12, ehc_args_int(&args, "n", 0));
  TEST_ASSERT_EQUAL(0, ehc_args_bool(&args, "b", 1));
  TEST_ASSERT_EQUAL_STRING("a\"b", ehc_args_str(&args, "s", ""));
  TEST_ASSERT_NOT_NULL(ehc_args_raw(&args, "n"));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_dispatch_and_args);
  RUN_TEST(test_unknown_command_fails);
  RUN_TEST(test_defer_after_ack);
  RUN_TEST(test_reject_force_checkin_in_table);
  RUN_TEST(test_args_coercion_and_escape);
  return UNITY_END();
}
