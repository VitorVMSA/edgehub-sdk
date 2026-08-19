#include "ehc_test_support.h"

static const ehc_metric_t s_metrics[] = {
  EHC_METRIC_NUM("temperature"),
};

void setUp(void)
{
  ehc_test_begin();
}

void tearDown(void)
{
  ehc_shutdown();
}

static void test_queue_until_active(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_metrics;
  u.metric_count = 1;
  ehc_host_store_put("ehc.hmac.secret", "test-secret");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_FULL, ehc_get_readiness());
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 7));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  TEST_ASSERT_NULL(ehc_host_find_url("/device/telemetry"));
  ehc_host_push_reply("/device/checkin", 200, "{\"provisioningStatus\":\"active\"}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(EHC_ACTIVE, ehc_get_readiness());
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tick());
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/telemetry"));
}

static void test_401_returns_to_full(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_metrics;
  u.metric_count = 1;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  ehc_host_clear_captures();
  ehc_host_push_reply(
    "/device/telemetry",
    401,
    "{\"code\":\"TELEMETRY_DEVICE_CREDENTIAL_INVALID\"}");
  (void)ehc_tel_publish();
  TEST_ASSERT_EQUAL(EHC_FULL, ehc_get_readiness());
  TEST_ASSERT(strstr(ehc_not_ready_reason(), "reprovision") != NULL);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_queue_until_active);
  RUN_TEST(test_401_returns_to_full);
  return UNITY_END();
}
