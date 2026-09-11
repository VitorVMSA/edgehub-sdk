#include "ehc_test_support.h"

static int s_apply_ok;
static int s_apply_calls;

static int s_dl_calls;
static char s_dl_key[32];
static char s_dl_ver[16];
static char s_dl_url[64];
static char s_dl_sum[16];

static int apply_fn(
  const char *asset_key, const char *version, const char *source, const char *payload, void *ctx)
{
  (void)asset_key;
  (void)version;
  (void)source;
  (void)payload;
  (void)ctx;
  s_apply_calls++;
  return s_apply_ok;
}

static void download_fn(const char *asset_key, const char *version, const char *url, const char *sum)
{
  s_dl_calls++;
  strncpy(s_dl_key, asset_key ? asset_key : "", sizeof(s_dl_key) - 1);
  strncpy(s_dl_ver, version ? version : "", sizeof(s_dl_ver) - 1);
  strncpy(s_dl_url, url ? url : "", sizeof(s_dl_url) - 1);
  strncpy(s_dl_sum, sum ? sum : "", sizeof(s_dl_sum) - 1);
}

void setUp(void)
{
  ehc_test_begin();
  s_apply_ok = 0;
  s_apply_calls = 0;
  s_dl_calls = 0;
  s_dl_key[0] = '\0';
  s_dl_ver[0] = '\0';
  s_dl_url[0] = '\0';
  s_dl_sum[0] = '\0';
}

void tearDown(void)
{
  ehc_shutdown();
}

static void init_with_apply(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.on_config_apply = apply_fn;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
}

static void test_version_not_stored_on_apply_fail(void)
{
  char ver[64];
  init_with_apply();
  s_apply_ok = 0;
  ehc_host_push_reply(
    "/device/checkin",
    200,
    "{\"provisioningStatus\":\"active\","
    "\"configurations\":{\"internalConfigurations\":{\"version\":\"3\",\"source\":\"mdm\",\"payload\":{}}}}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(1, s_apply_calls);
  TEST_ASSERT_EQUAL(0, ehc_host_store_get("ehc.asset.internalConfigurations", ver, sizeof(ver)));
}

static void test_version_stored_on_apply_ok(void)
{
  char ver[64];
  init_with_apply();
  s_apply_ok = 1;
  ehc_host_push_reply(
    "/device/checkin",
    200,
    "{\"provisioningStatus\":\"active\","
    "\"configurations\":{\"internalConfigurations\":{\"version\":\"3\",\"source\":\"mdm\",\"payload\":{}}}}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(1, ehc_host_store_get("ehc.asset.internalConfigurations", ver, sizeof(ver)));
  TEST_ASSERT_EQUAL_STRING("3", ver);
}

static void test_storage_write_fail_propagated(void)
{
  char ver[64];
  init_with_apply();
  s_apply_ok = 1;
  ehc_host_set_store_fail_write(1);
  ehc_host_push_reply(
    "/device/checkin",
    200,
    "{\"provisioningStatus\":\"active\","
    "\"configurations\":{\"internalConfigurations\":{\"version\":\"4\",\"source\":\"mdm\",\"payload\":{}}}}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  ehc_host_set_store_fail_write(0);
  TEST_ASSERT_EQUAL(0, ehc_host_store_get("ehc.asset.internalConfigurations", ver, sizeof(ver)));
}

static void test_download_callback_invoked(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.on_download = download_fn;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_host_push_reply(
    "/device/checkin",
    200,
    "{\"provisioningStatus\":\"active\","
    "\"downloads\":{\"fw\":{\"version\":\"2\",\"downloadUrl\":\"https://x/a.bin\",\"checksumSha256\":\"aa\"}}}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(1, s_dl_calls);
  TEST_ASSERT_EQUAL_STRING("fw", s_dl_key);
  TEST_ASSERT_EQUAL_STRING("2", s_dl_ver);
  TEST_ASSERT_EQUAL_STRING("https://x/a.bin", s_dl_url);
  TEST_ASSERT_EQUAL_STRING("aa", s_dl_sum);
}

static void test_download_without_callback_ok(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_host_push_reply(
    "/device/checkin",
    200,
    "{\"provisioningStatus\":\"active\","
    "\"downloads\":{\"fw\":{\"version\":\"2\",\"downloadUrl\":\"https://x/a.bin\",\"checksumSha256\":\"aa\"}}}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(0, s_dl_calls);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_version_not_stored_on_apply_fail);
  RUN_TEST(test_version_stored_on_apply_ok);
  RUN_TEST(test_storage_write_fail_propagated);
  RUN_TEST(test_download_callback_invoked);
  RUN_TEST(test_download_without_callback_ok);
  return UNITY_END();
}
