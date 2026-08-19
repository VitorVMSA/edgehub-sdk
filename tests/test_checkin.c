#include "ehc_test_support.h"

static int fw_fn(char *out, size_t cap)
{
  strncpy(out, "1.2.3", cap - 1);
  out[cap - 1] = '\0';
  return 1;
}

static const ehc_checkin_key_t s_keys[] = {
  EHC_KEY_CONST("boardModel", "ecopro_v3"),
  EHC_KEY_FN("firmware", fw_fn, EHC_KEY_REQUIRED),
};

void setUp(void)
{
  ehc_test_begin();
}

void tearDown(void)
{
  ehc_shutdown();
}

static void test_checkin_keys_in_body(void)
{
  ehc_user_t u;
  const ehc_host_http_capture_t *cap;
  ehc_test_fill_user(&u);
  u.checkin_keys = s_keys;
  u.checkin_key_count = 2;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  cap = ehc_host_find_url("/device/checkin");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "\"boardModel\":\"ecopro_v3\"") != NULL);
  TEST_ASSERT(strstr(cap->body, "\"firmware\":\"1.2.3\"") != NULL);
  TEST_ASSERT(strstr(cap->body, "\"deviceUid\":\"dev-1\"") != NULL);
}

static void test_asset_key_from_storage(void)
{
  ehc_user_t u;
  const ehc_host_http_capture_t *cap;
  static const ehc_checkin_key_t keys[] = {
    EHC_KEY_ASSET("classifications"),
  };
  ehc_test_fill_user(&u);
  u.checkin_keys = keys;
  u.checkin_key_count = 1;
  ehc_host_store_put("ehc.asset.classifications", "v9");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  cap = ehc_host_find_url("/device/checkin");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "\"classifications\":\"v9\"") != NULL);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_checkin_keys_in_body);
  RUN_TEST(test_asset_key_from_storage);
  return UNITY_END();
}
