#include "unity.h"
#include "eh_test_harness.h"

static eh_test_ctx_t g_ctx;
static eh_client_t g_cli;

void setUp(void)
{
  eh_test_ctx_reset(&g_ctx);
}

void tearDown(void) {}

static void init_with_ex(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_fill_cfg(&cfg);
  cfg.backoff_initial_ms = 2000;
  cfg.backoff_max_ms = 8000;
  cfg.checkin_interval_ms = 60000;
  cfg.telemetry_interval_ms = 10000;
  cfg.telemetry_manual_only = 0;
  memset(&cb, 0, sizeof(cb));
  cb.http_post_ex = eh_test_http_post_ex;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
}

static void test_retry_after_overrides_backoff(void)
{
  eh_client_status_t st;
  eh_test_http_reply_t *r;
  init_with_ex();
  eh_test_push_reply(&g_ctx, "/device/telemetry", 429, "{}");
  r = &g_ctx.replies[g_ctx.reply_count - 1];
  r->retry_after_sec = 7;
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 100));
  eh_client_get_status(&g_cli, &st);
  TEST_ASSERT_EQUAL(7100, (int)st.backoff_until_ms);
}

static void test_tick_noop_during_backoff(void)
{
  eh_test_http_reply_t *r;
  int before;
  init_with_ex();
  eh_test_push_reply(&g_ctx, "/device/telemetry", 429, "{}");
  r = &g_ctx.replies[g_ctx.reply_count - 1];
  r->retry_after_sec = 7;
  (void)eh_client_force_telemetry(&g_cli, 100);
  before = g_ctx.capture_count;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_tick(&g_cli, 200));
  TEST_ASSERT_EQUAL(before, g_ctx.capture_count);
}

static void test_exponential_backoff_caps(void)
{
  eh_client_status_t st;
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_fill_cfg(&cfg);
  cfg.backoff_initial_ms = 2000;
  cfg.backoff_max_ms = 5000;
  cfg.telemetry_manual_only = 1;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  g_ctx.force_offline = 1;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "s", NULL));
  (void)eh_client_force_telemetry(&g_cli, 0);
  eh_client_get_status(&g_cli, &st);
  TEST_ASSERT(st.backoff_until_ms >= 2000);
  (void)eh_client_force_telemetry(&g_cli, st.backoff_until_ms);
  eh_client_get_status(&g_cli, &st);
  TEST_ASSERT(st.backoff_current_ms <= 5000);
}

static void test_retry_after_on_401(void)
{
  eh_client_status_t st;
  eh_test_http_reply_t *r;
  init_with_ex();
  eh_test_push_reply(&g_ctx, "/device/telemetry", 401, "{}");
  r = &g_ctx.replies[g_ctx.reply_count - 1];
  r->retry_after_sec = 11;
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 100));
  eh_client_get_status(&g_cli, &st);
  TEST_ASSERT_EQUAL(11100, (int)st.backoff_until_ms);
}

static void test_success_resets_backoff(void)
{
  eh_client_status_t st;
  init_with_ex();
  g_ctx.force_offline = 1;
  (void)eh_client_force_telemetry(&g_cli, 0);
  g_ctx.force_offline = 0;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 100000));
  eh_client_get_status(&g_cli, &st);
  TEST_ASSERT_EQUAL(2000, (int)st.backoff_current_ms);
}

static void test_tick_sends_telemetry_after_checkin_revoked(void)
{
  int i;
  int saw_tel = 0;
  eh_config_t cfg;
  eh_callbacks_t cb;

  eh_test_fill_cfg(&cfg);
  cfg.telemetry_manual_only = 0;
  cfg.checkin_interval_ms = 60000;
  cfg.telemetry_interval_ms = 10000;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    401,
    "{\"code\":\"CHECKIN_DEVICE_CREDENTIAL_REVOKED\"}");
  eh_test_push_reply(&g_ctx, "/device/telemetry", 200, "{}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_tick(&g_cli, 10000));
  for (i = 0; i < g_ctx.capture_count; i++) {
    if (strstr(g_ctx.captures[i].url, "/device/telemetry")) {
      saw_tel = 1;
    }
  }
  TEST_ASSERT_EQUAL(1, saw_tel);
  TEST_ASSERT_EQUAL(1, g_cli.needs_credential_reprovision);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_retry_after_overrides_backoff);
  RUN_TEST(test_retry_after_on_401);
  RUN_TEST(test_tick_noop_during_backoff);
  RUN_TEST(test_exponential_backoff_caps);
  RUN_TEST(test_success_resets_backoff);
  RUN_TEST(test_tick_sends_telemetry_after_checkin_revoked);
  return UNITY_END();
}
