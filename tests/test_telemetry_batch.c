#include "ehc_test_support.h"

static const ehc_metric_t s_metrics[] = {
  EHC_METRIC_NUM("temperature"),
};

static void init_active_batch(uint8_t batch_max)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_metrics;
  u.metric_count = 1;
  u.telemetry_manual_only = 1;
  u.telemetry_batch_max = batch_max;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
}

void setUp(void)
{
  ehc_test_begin();
}

void tearDown(void)
{
  ehc_shutdown();
}

static void test_enqueue_without_flush_then_batch_post(void)
{
  const ehc_host_http_capture_t *cap;
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 2));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:01.000Z"));
  TEST_ASSERT_EQUAL(2, ehc_tel_queued());
  ehc_host_clear_captures();
  TEST_ASSERT_NULL(ehc_host_find_url("/device/telemetry"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
  cap = ehc_host_find_url("/device/telemetry");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT_EQUAL(1, ehc_host_capture_count());
  TEST_ASSERT(strstr(cap->body, "\"readings\"") != NULL);
  TEST_ASSERT(strstr(cap->body, "2024-06-01T12:00:00.000Z") != NULL);
  TEST_ASSERT(strstr(cap->body, "2024-06-01T12:00:01.000Z") != NULL);
}

static void test_enqueue_null_uses_port_clock(void)
{
  const ehc_host_http_capture_t *cap;
  init_active_batch(8);
  ehc_host_clock_set(100);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 3));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue(NULL));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  cap = ehc_host_find_url("/device/telemetry");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT(strstr(cap->body, "1970-01-01T00:00:00.100Z") != NULL);
}

static void test_tick_does_not_flush_when_manual(void)
{
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 4));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:02.000Z"));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tick());
  TEST_ASSERT_EQUAL(1, ehc_tel_queued());
  TEST_ASSERT_NULL(ehc_host_find_url("/device/telemetry"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/telemetry"));
}

static void test_publish_still_sends_immediately(void)
{
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 5));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/telemetry"));
}

static void test_queue_until_active_flush_not_tick(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_metrics;
  u.metric_count = 1;
  u.telemetry_manual_only = 1;
  u.telemetry_batch_max = 8;
  ehc_host_store_put("ehc.hmac.secret", "test-secret");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_FULL, ehc_get_readiness());
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 7));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:03.000Z"));
  TEST_ASSERT_EQUAL(1, ehc_tel_queued());
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  TEST_ASSERT_NULL(ehc_host_find_url("/device/telemetry"));
  TEST_ASSERT_EQUAL(1, ehc_tel_queued());
  ehc_host_push_reply("/device/checkin", 200, "{\"provisioningStatus\":\"active\"}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(EHC_ACTIVE, ehc_get_readiness());
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tick());
  TEST_ASSERT_NULL(ehc_host_find_url("/device/telemetry"));
  TEST_ASSERT_EQUAL(1, ehc_tel_queued());
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  TEST_ASSERT_NOT_NULL(ehc_host_find_url("/device/telemetry"));
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
}

static void test_queued_zero_before_init(void)
{
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_enqueue_without_flush_then_batch_post);
  RUN_TEST(test_enqueue_null_uses_port_clock);
  RUN_TEST(test_tick_does_not_flush_when_manual);
  RUN_TEST(test_publish_still_sends_immediately);
  RUN_TEST(test_queue_until_active_flush_not_tick);
  RUN_TEST(test_queued_zero_before_init);
  return UNITY_END();
}
