#include "ehc_test_support.h"

static int s_apply_ok;
static int s_apply_calls;

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

void setUp(void)
{
  ehc_test_begin();
  s_apply_ok = 0;
  s_apply_calls = 0;
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

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_version_not_stored_on_apply_fail);
  RUN_TEST(test_version_stored_on_apply_ok);
  RUN_TEST(test_storage_write_fail_propagated);
  return UNITY_END();
}
