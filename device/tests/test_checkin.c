#include "unity.h"
#include "eh_test_harness.h"

static eh_test_ctx_t g_ctx;
static eh_client_t g_cli;

void setUp(void)
{
  eh_test_ctx_reset(&g_ctx);
}

void tearDown(void) {}

static void init_cli(int with_group)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_fill_cfg(&cfg);
  if (with_group) {
    strncpy(cfg.group_key, "gk", sizeof(cfg.group_key) - 1);
    strncpy(cfg.product_code, "pc", sizeof(cfg.product_code) - 1);
  }
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.checkin_keys_collect = eh_test_checkin_keys_collect;
  cb.on_checkin_download = eh_test_on_download;
  cb.on_checkin_configuration = eh_test_on_configuration;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
}

static const eh_test_http_capture_t *find_checkin(void)
{
  int i;
  for (i = 0; i < g_ctx.capture_count; i++) {
    if (strstr(g_ctx.captures[i].url, "/device/checkin")) {
      return &g_ctx.captures[i];
    }
  }
  return NULL;
}

static void test_minimal_body_omits_group(void)
{
  const eh_test_http_capture_t *cap;
  g_ctx.checkin_key_name = "firmware";
  g_ctx.checkin_key_value = "1.0.0";
  init_cli(0);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  cap = find_checkin();
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "\"groupKey\"") == NULL);
  TEST_ASSERT(strstr(cap->body, "\"productCode\"") == NULL);
  TEST_ASSERT(strstr(cap->body, "\"deviceUid\":\"dev-1\"") != NULL);
  TEST_ASSERT(strstr(cap->body, "\"firmware\":\"1.0.0\"") != NULL);
}

static void test_full_body_includes_group(void)
{
  const eh_test_http_capture_t *cap;
  init_cli(1);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  cap = find_checkin();
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "\"groupKey\":\"gk\"") != NULL);
  TEST_ASSERT(strstr(cap->body, "\"productCode\":\"pc\"") != NULL);
}

static void test_set_checkin_keys_json(void)
{
  const eh_test_http_capture_t *cap;
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_fill_cfg(&cfg);
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_checkin_keys_json(&g_cli, "{\"boardModel\":\"x\"}"));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  cap = find_checkin();
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "\"boardModel\":\"x\"") != NULL);
}

static void test_provisioning_status_active(void)
{
  eh_test_push_reply(&g_ctx, "/device/checkin", 200, "{\"provisioningStatus\":\"active\"}");
  init_cli(0);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(EH_PROV_ACTIVE, eh_client_get_provisioning(&g_cli));
}

static void test_keys_drift_forces_checkin(void)
{
  init_cli(0);
  g_ctx.checkin_key_name = "firmware";
  g_ctx.checkin_key_value = "1";
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  g_ctx.capture_count = 0;
  g_ctx.checkin_key_value = "2";
  TEST_ASSERT_EQUAL(EH_OK, eh_client_tick(&g_cli, 10));
  TEST_ASSERT_NOT_NULL(find_checkin());
}

static void test_downloads_and_configurations(void)
{
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    200,
    "{\"downloads\":{\"fw\":{\"version\":\"2\",\"downloadUrl\":\"https://x/a.bin\",\"checksumSha256\":\"aa\"}},"
    "\"configurations\":{\"internalConfigurations\":{\"version\":\"1\",\"source\":\"mdm\",\"payload\":\"{}\"}}}");
  init_cli(0);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_ctx.download_calls);
  TEST_ASSERT_EQUAL(1, g_ctx.config_calls);
}

static void test_download_without_checksum_and_config_without_source(void)
{
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    200,
    "{\"downloads\":{\"fw\":{\"version\":\"2\",\"downloadUrl\":\"https://x/a.bin\"}},"
    "\"configurations\":{\"internalConfigurations\":{\"version\":\"1\",\"payload\":{}}}}");
  init_cli(0);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_ctx.download_calls);
  TEST_ASSERT_EQUAL_STRING("", g_ctx.last_checksum);
  TEST_ASSERT_EQUAL(1, g_ctx.config_calls);
  TEST_ASSERT_EQUAL_STRING("", g_ctx.last_source);
}

static void init_cli_identity(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  uint8_t seed[32];
  eh_test_fill_cfg(&cfg);
  cfg.enable_identity_credential_issue = 1;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.checkin_keys_collect = eh_test_checkin_keys_collect;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  memset(seed, 0x11, sizeof(seed));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_identity_seed(&g_cli, seed));
}

static void test_identity_challenge_pretty_json(void)
{
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    200,
    "{\"identityChallenge\":{\"challengeId\": \"ch-1\",\"nonce\": \"nn-1\"}}");
  eh_test_push_reply(
    &g_ctx,
    "/credential/issue",
    200,
    "{\"credential\":{\"secret\":\"issued-hmac\"}}");
  init_cli_identity();
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_cli.has_secret_current);
  TEST_ASSERT_EQUAL_STRING("issued-hmac", g_cli.secret_current);
}

static void test_identity_challenge_on_http_401(void)
{
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    401,
    "{\"identityChallenge\":{\"challengeId\":\"ch-1\",\"nonce\":\"nn-1\"}}");
  eh_test_push_reply(
    &g_ctx,
    "/credential/issue",
    200,
    "{\"credential\":{\"secret\":\"issued-from-401\"}}");
  init_cli_identity();
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_cli.has_secret_current);
  TEST_ASSERT_EQUAL_STRING("issued-from-401", g_cli.secret_current);
}

static void test_expires_at_marks_reprovision_on_tick(void)
{
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    200,
    "{\"identityChallenge\":{\"challengeId\":\"ch-1\",\"nonce\":\"nn-1\"}}");
  eh_test_push_reply(
    &g_ctx,
    "/credential/issue",
    200,
    "{\"credential\":{\"secret\":\"issued-hmac\",\"expiresAt\":\"1970-01-01T00:00:01.000Z\"}}");
  init_cli_identity();
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(1000, (int)g_cli.credential_expires_at_ms);
  TEST_ASSERT_EQUAL(0, g_cli.needs_credential_reprovision);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_tick(&g_cli, 999));
  TEST_ASSERT_EQUAL(0, g_cli.needs_credential_reprovision);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_tick(&g_cli, 1000));
  TEST_ASSERT_EQUAL(1, g_cli.needs_credential_reprovision);
}

static void test_identity_not_registered_backoff(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_client_status_t st;
  eh_test_fill_cfg(&cfg);
  cfg.backoff_initial_ms = 2000;
  cfg.backoff_max_ms = 8000;
  cfg.enable_identity_credential_issue = 1;
  memset(&cb, 0, sizeof(cb));
  cb.http_post_ex = eh_test_http_post_ex;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    401,
    "{\"code\":\"CHECKIN_DEVICE_IDENTITY_NOT_REGISTERED\",\"error\":\"Device identity not registered\"}");
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 100));
  eh_client_get_status(&g_cli, &st);
  TEST_ASSERT_EQUAL(300100, (int)st.backoff_until_ms);
}

static void test_revoked_sets_reprovision(void)
{
  init_cli(0);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "old-secret", NULL));
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    401,
    "{\"code\":\"CHECKIN_DEVICE_CREDENTIAL_REVOKED\"}");
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_cli.needs_credential_reprovision);
}

static void test_required_sets_reprovision(void)
{
  init_cli(0);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "old-secret", NULL));
  eh_test_push_reply(
    &g_ctx,
    "/device/checkin",
    401,
    "{\"code\":\"CHECKIN_DEVICE_CREDENTIAL_REQUIRED\"}");
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, g_cli.needs_credential_reprovision);
}

static void test_provisioning_status_active_pretty_json(void)
{
  eh_test_push_reply(&g_ctx, "/device/checkin", 200, "{\"provisioningStatus\": \"active\"}");
  init_cli(0);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_checkin(&g_cli, 0));
  TEST_ASSERT_EQUAL(EH_PROV_ACTIVE, eh_client_get_provisioning(&g_cli));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_minimal_body_omits_group);
  RUN_TEST(test_full_body_includes_group);
  RUN_TEST(test_set_checkin_keys_json);
  RUN_TEST(test_provisioning_status_active);
  RUN_TEST(test_provisioning_status_active_pretty_json);
  RUN_TEST(test_keys_drift_forces_checkin);
  RUN_TEST(test_downloads_and_configurations);
  RUN_TEST(test_download_without_checksum_and_config_without_source);
  RUN_TEST(test_identity_challenge_pretty_json);
  RUN_TEST(test_identity_challenge_on_http_401);
  RUN_TEST(test_expires_at_marks_reprovision_on_tick);
  RUN_TEST(test_identity_not_registered_backoff);
  RUN_TEST(test_revoked_sets_reprovision);
  RUN_TEST(test_required_sets_reprovision);
  return UNITY_END();
}
