#include "ehc_test_support.h"

static int s_fw_ok;
static int fw_fn(char *out, size_t cap)
{
  if (!s_fw_ok) {
    return 0;
  }
  strncpy(out, "1.0.0", cap - 1);
  out[cap - 1] = '\0';
  return 1;
}

static const ehc_checkin_key_t s_keys_required[] = {
  EHC_KEY_FN("firmware", fw_fn, EHC_KEY_REQUIRED),
};

void setUp(void)
{
  ehc_test_begin();
  s_fw_ok = 0;
}

void tearDown(void)
{
  ehc_shutdown();
}

static void test_missing_base_url_no_http(void)
{
  ehc_user_t u;
  ehc_test_fill_user(&u);
  u.base_url = (ehc_identity_source_t)EHC_IDENT_NONE;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_UNCONFIGURED, ehc_get_readiness());
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tick());
  TEST_ASSERT_EQUAL(0, ehc_host_capture_count());
}

static void test_missing_device_uid_no_http(void)
{
  ehc_user_t u;
  ehc_test_fill_user(&u);
  u.device_uid = (ehc_identity_source_t)EHC_IDENT_NONE;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tick());
  TEST_ASSERT_EQUAL(0, ehc_host_capture_count());
}

static void test_required_key_blocks_checkin(void)
{
  ehc_user_t u;
  ehc_test_fill_user(&u);
  u.checkin_keys = s_keys_required;
  u.checkin_key_count = 1;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_UNCONFIGURED, ehc_get_readiness());
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tick());
  TEST_ASSERT_EQUAL(0, ehc_host_capture_count());
  s_fw_ok = 1;
  TEST_ASSERT_EQUAL(EHC_MINIMAL, ehc_get_readiness());
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tick());
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/checkin"));
}

static void test_not_ready_log_throttled(void)
{
  ehc_user_t u;
  int n1;
  ehc_test_fill_user(&u);
  u.base_url = (ehc_identity_source_t)EHC_IDENT_NONE;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tick());
  n1 = ehc_host_log_count_containing("not ready");
  TEST_ASSERT_EQUAL(1, n1);
  ehc_test_clock_advance(1000);
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tick());
  TEST_ASSERT_EQUAL(n1, ehc_host_log_count_containing("not ready"));
  ehc_test_clock_advance(EHC_NOT_READY_LOG_MS);
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tick());
  TEST_ASSERT_EQUAL(n1 + 1, ehc_host_log_count_containing("not ready"));
}

static void test_minimal_then_full(void)
{
  ehc_user_t u;
  const ehc_host_http_capture_t *cap;
  ehc_test_fill_user(&u);
  u.group_key = (ehc_identity_source_t)EHC_IDENT_STORE("gk");
  u.product_code = (ehc_identity_source_t)EHC_IDENT_STORE("pc");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_MINIMAL, ehc_get_readiness());
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tick());
  cap = ehc_host_find_url("/device/checkin");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "\"groupKey\"") == NULL);
  TEST_ASSERT(strstr(cap->body, "\"productCode\"") == NULL);
  ehc_host_clear_captures();
  ehc_host_store_put("gk", "group-a");
  ehc_host_store_put("pc", "prod-b");
  TEST_ASSERT_EQUAL(EHC_FULL, ehc_get_readiness());
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  cap = ehc_host_find_url("/device/checkin");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "\"groupKey\":\"group-a\"") != NULL);
  TEST_ASSERT(strstr(cap->body, "\"productCode\":\"prod-b\"") != NULL);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_missing_base_url_no_http);
  RUN_TEST(test_missing_device_uid_no_http);
  RUN_TEST(test_required_key_blocks_checkin);
  RUN_TEST(test_not_ready_log_throttled);
  RUN_TEST(test_minimal_then_full);
  return UNITY_END();
}
