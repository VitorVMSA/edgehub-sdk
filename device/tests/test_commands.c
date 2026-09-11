#include "unity.h"
#include "eh_test_harness.h"

static eh_test_ctx_t g_ctx;
static eh_client_t g_cli;

void setUp(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_ctx_reset(&g_ctx);
  eh_test_fill_cfg(&cfg);
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.on_command = eh_test_on_command;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
}

void tearDown(void) {}

static void test_force_checkin_builtin_skips_on_command(void)
{
  eh_test_push_reply(
    &g_ctx,
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e1\",\"commandKey\":\"device.force_checkin\",\"payload\":{}}]}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(0, g_ctx.on_command_calls);
  TEST_ASSERT(strstr(g_ctx.captures[g_ctx.capture_count - 1].url, "/checkin") != NULL || g_ctx.capture_count >= 2);
}

static void test_user_command_dispatched_and_confirmed(void)
{
  int i;
  int executed = 0;
  eh_test_push_reply(
    &g_ctx,
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e2\",\"commandKey\":\"motor.set\",\"payload\":{\"rpm\":10}}]}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_ctx.on_command_calls);
  TEST_ASSERT_EQUAL_STRING("motor.set", g_ctx.last_command_key);
  for (i = 0; i < g_ctx.capture_count; i++) {
    if (strstr(g_ctx.captures[i].url, "/command-executions/")) {
      executed++;
    }
  }
  TEST_ASSERT_EQUAL(1, executed);
}

static void test_extra_commands_dropped(void)
{
  eh_test_push_reply(
    &g_ctx,
    "/device/telemetry",
    201,
    "{\"pendingCommands\":["
    "{\"executionId\":\"a\",\"commandKey\":\"c1\",\"payload\":{}},"
    "{\"executionId\":\"b\",\"commandKey\":\"c2\",\"payload\":{}},"
    "{\"executionId\":\"c\",\"commandKey\":\"c3\",\"payload\":{}},"
    "{\"executionId\":\"d\",\"commandKey\":\"c4\",\"payload\":{}},"
    "{\"executionId\":\"e\",\"commandKey\":\"c5\",\"payload\":{}},"
    "{\"executionId\":\"f\",\"commandKey\":\"c6\",\"payload\":{}}"
    "]}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(5, g_ctx.on_command_calls);
}

static void test_poll_downlink_with_telemetry_disabled(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_ctx_reset(&g_ctx);
  eh_test_fill_cfg(&cfg);
  cfg.enable_telemetry = 0;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.on_command = eh_test_on_command;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(0, g_ctx.capture_count);
  eh_test_push_reply(
    &g_ctx,
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e-dl\",\"commandKey\":\"motor.x\",\"payload\":{}}]}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_poll_downlink(&g_cli, 0));
  TEST_ASSERT(g_ctx.capture_count >= 1);
  TEST_ASSERT_NOT_NULL(strstr(g_ctx.captures[0].url, "/device/telemetry"));
  TEST_ASSERT_NOT_NULL(strstr(g_ctx.captures[0].body, "\"values\":{}"));
  TEST_ASSERT_EQUAL(1, g_ctx.on_command_calls);
}

static void test_null_command_key_dispatched_and_confirmed(void)
{
  int i;
  int executed = 0;
  eh_test_push_reply(
    &g_ctx,
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e-null\",\"commandKey\":null,\"payload\":{}}]}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_ctx.on_command_calls);
  TEST_ASSERT_EQUAL_STRING("", g_ctx.last_command_key);
  for (i = 0; i < g_ctx.capture_count; i++) {
    if (strstr(g_ctx.captures[i].url, "/command-executions/")) {
      executed++;
    }
  }
  TEST_ASSERT_EQUAL(1, executed);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_force_checkin_builtin_skips_on_command);
  RUN_TEST(test_user_command_dispatched_and_confirmed);
  RUN_TEST(test_extra_commands_dropped);
  RUN_TEST(test_poll_downlink_with_telemetry_disabled);
  RUN_TEST(test_null_command_key_dispatched_and_confirmed);
  return UNITY_END();
}
