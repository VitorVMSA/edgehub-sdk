#include "ehc_test_support.h"
#include "ehc_internal.h"

static const ehc_metric_t s_metrics[] = {
  EHC_METRIC_NUM("temperature"),
};

static int s_cmd_calls;

static int cmd_ok(const ehc_args_t *a, ehc_cmd_result_t *r, void *ctx)
{
  (void)a;
  (void)ctx;
  s_cmd_calls++;
  return ehc_cmd_ok(r, "ok");
}

static const ehc_command_t s_cmds[] = {
  EHC_COMMAND("motor.set_speed", cmd_ok),
};

static int count_url(const char *substr)
{
  int i;
  int n = 0;
  for (i = 0; i < ehc_host_capture_count(); i++) {
    if (strstr(ehc_host_capture(i)->url, substr) != NULL) {
      n++;
    }
  }
  return n;
}

void setUp(void)
{
  ehc_test_begin();
  s_cmd_calls = 0;
}

void tearDown(void)
{
  ehc_shutdown();
}

static void test_disable_telemetry_no_post(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.disable_telemetry = 1;
  u.metrics = NULL;
  u.metric_count = 0;
  u.telemetry_manual_only = 0;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tel_set_num("temperature", 1));
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tel_publish());
  ehc_test_become_active();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tick());
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tel_set_num("temperature", 2));
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tel_enqueue(NULL));
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tel_flush());
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tel_publish());
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/checkin"));
  TEST_ASSERT_NULL(ehc_host_find_url("/device/telemetry"));
  TEST_ASSERT_EQUAL(0, count_url("/device/telemetry"));
}

static void test_poll_commands_posts_empty_values(void)
{
  ehc_user_t u;
  const ehc_host_http_capture_t *cap;
  ehc_test_fill_user_full(&u);
  u.disable_telemetry = 1;
  u.metrics = NULL;
  u.metric_count = 0;
  u.poll_commands = 1;
  u.commands = s_cmds;
  u.command_count = 1;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
  ehc_host_clear_captures();
  ehc_host_push_reply(
    "/device/telemetry",
    201,
    "{\"pendingCommands\":[{\"executionId\":\"e-poll\",\"commandKey\":\"motor.set_speed\",\"payload\":{}}]}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tick());
  cap = ehc_host_find_url("/device/telemetry");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT_NOT_NULL(strstr(cap->body, "\"values\":{}"));
  TEST_ASSERT_EQUAL(1, s_cmd_calls);
  TEST_ASSERT_EQUAL(1, count_url("/device/telemetry"));
}

static void test_disable_commands_empty_table(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.disable_commands = 1;
  u.commands = NULL;
  u.command_count = 0;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/checkin"));
}

static void test_telemetry_still_posts_without_flag(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_metrics;
  u.metric_count = 1;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/telemetry"));
}

static void test_all_disable_rejected(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.disable_telemetry = 1;
  u.disable_commands = 1;
  u.disable_checkin = 1;
  u.metrics = NULL;
  u.metric_count = 0;
  u.commands = NULL;
  u.command_count = 0;
  TEST_ASSERT_EQUAL(EHC_ECONFIG, ehc_test_init(&u));
}

static void test_pkcs8_auto_enables_identity_issue(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.identity_pkcs8 = (ehc_identity_source_t)EHC_IDENT_LIT("dGVzdA==");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(1, g_ehc.eh.cfg.enable_identity_credential_issue);
}

static void test_runtime_wrappers(void)
{
  ehc_user_t u;
  ehc_device_status_t st;
  ehc_test_fill_user_full(&u);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_checkin_interval(12345));
  TEST_ASSERT_EQUAL(12345, (int)g_ehc.eh.cfg.checkin_interval_ms);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_telemetry_batch_max(4));
  TEST_ASSERT_EQUAL(4, (int)g_ehc.eh.cfg.telemetry_batch_max);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_telemetry_interval(5000));
  TEST_ASSERT_EQUAL(5000, (int)g_ehc.eh.cfg.telemetry_interval_ms);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_get_device_status(&st));
  TEST_ASSERT_EQUAL(EH_PROV_ACTIVE, st.provisioning);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_disable_telemetry_no_post);
  RUN_TEST(test_poll_commands_posts_empty_values);
  RUN_TEST(test_disable_commands_empty_table);
  RUN_TEST(test_telemetry_still_posts_without_flag);
  RUN_TEST(test_all_disable_rejected);
  RUN_TEST(test_pkcs8_auto_enables_identity_issue);
  RUN_TEST(test_runtime_wrappers);
  return UNITY_END();
}
