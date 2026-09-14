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
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
}

void tearDown(void) {}

static void test_hmac_single_telemetry_matches_node(void)
{
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_telemetry_ts(&g_cli, "2026-01-01T00:00:00.000Z"));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT(g_ctx.capture_count >= 1);
  TEST_ASSERT_EQUAL_STRING(
    "4c61fc86abc624e18c8b96ca2a3c37a4ea7291099b76e96ed349cf4c2f82e0b6",
    eh_test_header_sig(g_ctx.captures[0].headers));
  TEST_ASSERT(strstr(g_ctx.captures[0].body, "\"alias\"") == NULL);
  TEST_ASSERT(strstr(g_ctx.captures[0].body, "\"values\":{\"temp\":22.5}") != NULL);
}

static void test_hmac_batch_matches_node(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_ctx_reset(&g_ctx);
  eh_test_fill_cfg(&cfg);
  cfg.telemetry_batch_max = 2;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
  g_ctx.collect_num = 22.5;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2026-01-01T00:00:00.000Z", 0));
  g_ctx.collect_num = 23;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2026-01-01T00:00:01.000Z", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_flush_telemetry(&g_cli, 0));
  TEST_ASSERT(g_ctx.capture_count >= 1);
  TEST_ASSERT_EQUAL_STRING(
    "74f4517a89861f171d1bf9a135266cf82346994f23c8785bba02af6f4a73980f",
    eh_test_header_sig(g_ctx.captures[0].headers));
}

static void test_hmac_executed_matches_node(void)
{
  TEST_ASSERT_EQUAL(EH_OK, eh_client_confirm_command_execution(&g_cli, "exec-1", 1, NULL));
  TEST_ASSERT(g_ctx.capture_count >= 1);
  TEST_ASSERT(strstr(g_ctx.captures[0].url, "command-executions/exec-1/executed") != NULL);
  TEST_ASSERT_EQUAL_STRING(
    "991fdc5a9f59ee150ffc830843e67b096b1b9404e03bbbca3f57b3200be47130",
    eh_test_header_sig(g_ctx.captures[0].headers));
}

static void test_hmac_executed_failure_matches_node(void)
{
  TEST_ASSERT_EQUAL(EH_OK, eh_client_confirm_command_execution(&g_cli, "exec-1", 0, "nope"));
  TEST_ASSERT_EQUAL_STRING(
    "a0e3bb6f62412573d5364fc90a98a0a8c18a1671608edd32525b2b93ef8e6529",
    eh_test_header_sig(g_ctx.captures[0].headers));
}

static void test_hmac_small_float_matches_node_stringify(void)
{
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_telemetry_ts(&g_cli, "2026-01-01T00:00:00.000Z"));
  g_ctx.collect_num = 1e-5;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT(g_ctx.capture_count >= 1);
  TEST_ASSERT(strstr(g_ctx.captures[0].body, "\"values\":{\"temp\":0.00001}") != NULL);
  TEST_ASSERT_EQUAL_STRING(
    "c2c3a79fd8fe0de566bbb406c99374a2a3ce30ac201c15d8e67b71f30f256bce",
    eh_test_header_sig(g_ctx.captures[0].headers));
}

static void test_hmac_retry_previous_on_401_invalid(void)
{
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", "prev-secret"));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_telemetry_ts(&g_cli, "2026-01-01T00:00:00.000Z"));
  eh_test_push_reply(
    &g_ctx,
    "/device/telemetry",
    401,
    "{\"code\":\"TELEMETRY_DEVICE_CREDENTIAL_INVALID\"}");
  eh_test_push_reply(&g_ctx, "/device/telemetry", 200, "{}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(2, g_ctx.capture_count);
  TEST_ASSERT_EQUAL_STRING(
    "4c61fc86abc624e18c8b96ca2a3c37a4ea7291099b76e96ed349cf4c2f82e0b6",
    eh_test_header_sig(g_ctx.captures[0].headers));
  TEST_ASSERT_EQUAL_STRING(
    "0b8ee605ade28678e569d1caba1826b997d4bb86d1192a2285dc7f3927c1ede0",
    eh_test_header_sig(g_ctx.captures[1].headers));
  TEST_ASSERT_EQUAL(0, g_cli.needs_credential_reprovision);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_hmac_single_telemetry_matches_node);
  RUN_TEST(test_hmac_batch_matches_node);
  RUN_TEST(test_hmac_executed_matches_node);
  RUN_TEST(test_hmac_executed_failure_matches_node);
  RUN_TEST(test_hmac_small_float_matches_node_stringify);
  RUN_TEST(test_hmac_retry_previous_on_401_invalid);
  return UNITY_END();
}
