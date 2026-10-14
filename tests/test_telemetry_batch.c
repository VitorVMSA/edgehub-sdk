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

static const ehc_metric_t s_metrics_reset[] = {
  EHC_METRIC_NUM("temperature"),
  EHC_METRIC_STR("lastWasteTags"),
};

static void init_active_reset(uint8_t batch_max)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_metrics_reset;
  u.metric_count = 2;
  u.telemetry_manual_only = 1;
  u.telemetry_batch_max = batch_max;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
}

static void test_reset_omits_conditional_str(void)
{
  const ehc_host_http_capture_t *cap;
  const char *second_ts;
  init_active_reset(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_str("lastWasteTags", "TAG-A"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  ehc_tel_reset();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 2));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:01.000Z"));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  cap = ehc_host_find_url("/device/telemetry");
  TEST_ASSERT_NOT_NULL(cap);
  TEST_ASSERT_NOT_NULL(strstr(cap->body, "TAG-A"));
  second_ts = strstr(cap->body, "2024-06-01T12:00:01.000Z");
  TEST_ASSERT_NOT_NULL(second_ts);
  TEST_ASSERT_NULL(strstr(second_ts, "lastWasteTags"));
}

static void test_queue_clear_after_401(void)
{
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 2));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:01.000Z"));
  TEST_ASSERT_EQUAL(2, ehc_tel_queued());
  ehc_host_push_reply(
    "/device/telemetry",
    401,
    "{\"code\":\"TELEMETRY_INVALID_TS\",\"rejectedIndex\":0}");
  TEST_ASSERT_NOT_EQUAL(EHC_OK, ehc_tel_flush());
  TEST_ASSERT(ehc_tel_queued() > 0);
  ehc_tel_queue_clear();
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  TEST_ASSERT_NULL(ehc_host_find_url("/device/telemetry"));
}

static void test_flush_ok_delta_queued(void)
{
  uint8_t before;
  uint8_t sent;
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 2));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:01.000Z"));
  before = ehc_tel_queued();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  sent = (uint8_t)(before - ehc_tel_queued());
  TEST_ASSERT_EQUAL(2, sent);
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
}

static void test_queue_not_persisted_to_port(void)
{
  char blob[32];
  char secret[64];
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 2));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:01.000Z"));
  TEST_ASSERT_EQUAL(0, ehc_host_store_get("eh.tel.q", blob, sizeof(blob)));
  TEST_ASSERT_EQUAL(1, ehc_host_store_get("ehc.hmac.secret", secret, sizeof(secret)));
  TEST_ASSERT_EQUAL_STRING("test-secret", secret);
}

static void test_log_level_default_and_debug_setter(void)
{
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 321.5));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  TEST_ASSERT_EQUAL(0, ehc_host_log_count_containing("telemetry_enqueue"));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_log_level(EHC_LOG_DEBUG));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 654.25));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:01.000Z"));
  TEST_ASSERT_EQUAL(2, ehc_tel_queued());
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("telemetry_enqueue"));
  TEST_ASSERT_NOT_NULL(strstr(ehc_host_log_at(ehc_host_log_count() - 1), "status=EHC_OK(0)"));
  TEST_ASSERT_NOT_NULL(strstr(ehc_host_log_at(ehc_host_log_count() - 1), "queue_before=1 queue_after=2"));
}

static void test_debug_logs_are_sanitized_for_http_failures(void)
{
  const char *msg;
  int i;
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_log_level(EHC_LOG_DEBUG));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 987.125));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  ehc_host_push_reply_ex("/device/telemetry", 0, "", 0, 1);
  TEST_ASSERT_EQUAL(EHC_EIO, ehc_tel_flush());
  TEST_ASSERT_EQUAL(1, ehc_tel_queued());
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("result=transport_failure"));
  msg = ehc_host_log_at(ehc_host_log_count() - 1);
  TEST_ASSERT_NOT_NULL(strstr(msg, "queue_before=1 queue_after=1"));
  TEST_ASSERT_NULL(strstr(msg, "Authorization"));
  TEST_ASSERT_NULL(strstr(msg, "test-secret"));
  TEST_ASSERT_NULL(strstr(msg, "987.125"));
  TEST_ASSERT_NULL(strstr(msg, "temperature"));
  for (i = 0; i < ehc_host_log_count(); ++i) {
    msg = ehc_host_log_at(i);
    TEST_ASSERT_NULL(strstr(msg, "Authorization"));
    TEST_ASSERT_NULL(strstr(msg, "test-secret"));
    TEST_ASSERT_NULL(strstr(msg, "987.125"));
    TEST_ASSERT_NULL(strstr(msg, "temperature"));
  }
}

static void test_http_validation_code_and_success_queue_logs(void)
{
  const char *msg;
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_log_level(EHC_LOG_DEBUG));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 456.75));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  ehc_host_push_reply("/device/telemetry", 400,
    "{\"code\":\"TELEMETRY_INVALID_TS\",\"echo\":\"456.75\"}");
  TEST_ASSERT_EQUAL(EHC_EIO, ehc_tel_flush());
  TEST_ASSERT_EQUAL(1, ehc_tel_queued());
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("category=validation"));
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("http_status=400"));
  msg = ehc_host_log_at(ehc_host_log_count() - 1);
  TEST_ASSERT_NULL(strstr(msg, "456.75"));
  TEST_ASSERT_NULL(strstr(msg, "echo"));
  ehc_host_push_reply("/device/telemetry", 200, "{\"accepted\":true}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_flush());
  TEST_ASSERT_EQUAL(0, ehc_tel_queued());
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("telemetry_http result=accepted"));
  msg = ehc_host_log_at(ehc_host_log_count() - 1);
  TEST_ASSERT_NOT_NULL(strstr(msg, "queue_before=1 queue_after=0"));
  TEST_ASSERT_NULL(strstr(msg, "Authorization"));
  TEST_ASSERT_NULL(strstr(msg, "test-secret"));
}

static void test_unrecognized_http_response_is_not_logged(void)
{
  const char *msg;
  init_active_batch(8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_log_level(EHC_LOG_DEBUG));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 789.5));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  ehc_host_push_reply("/device/telemetry", 422,
    "{\"details\":{\"code\":\"TELEMETRY_INVALID_TS\"},\"value\":789.5,\"secret\":\"test-secret\"}");
  TEST_ASSERT_EQUAL(EHC_EIO, ehc_tel_flush());
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("category=unknown"));
  msg = ehc_host_log_at(ehc_host_log_count() - 1);
  TEST_ASSERT_NULL(strstr(msg, "TELEMETRY_INVALID_TS"));
  TEST_ASSERT_NULL(strstr(msg, "789.5"));
  TEST_ASSERT_NULL(strstr(msg, "test-secret"));
}

static void test_http_credential_classification_and_invalid_level(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_metrics;
  u.metric_count = 1;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
  TEST_ASSERT_EQUAL(EHC_EINVAL, ehc_set_log_level((ehc_log_level_t)99));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_log_level(EHC_LOG_DEBUG));
  ehc_host_store_put("ehc.hmac.secret", "test-secret");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 12.5));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  ehc_host_push_reply("/device/telemetry", 401,
    "{\"code\":\"TELEMETRY_DEVICE_CREDENTIAL_INVALID\",\"value\":12.5}");
  TEST_ASSERT_EQUAL(EHC_EIO, ehc_tel_flush());
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("category=credential"));
  TEST_ASSERT_NULL(strstr(ehc_host_log_at(ehc_host_log_count() - 1), "12.5"));
}

static void test_enqueue_not_ready_is_logged_before_flush(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.base_url = (ehc_identity_source_t)EHC_IDENT_NONE;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_set_log_level(EHC_LOG_DEBUG));
  TEST_ASSERT_EQUAL(EHC_ENOTREADY, ehc_tel_enqueue("2024-06-01T12:00:00.000Z"));
  TEST_ASSERT_EQUAL(1, ehc_host_log_count_containing("telemetry_enqueue"));
  TEST_ASSERT_NOT_NULL(strstr(ehc_host_log_at(ehc_host_log_count() - 1), "status=EHC_ENOTREADY(-2)"));
  TEST_ASSERT_NOT_NULL(strstr(ehc_host_log_at(ehc_host_log_count() - 1), "result=not_ready"));
  TEST_ASSERT_EQUAL(0, ehc_host_capture_count());
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
  RUN_TEST(test_reset_omits_conditional_str);
  RUN_TEST(test_queue_clear_after_401);
  RUN_TEST(test_flush_ok_delta_queued);
  RUN_TEST(test_queue_not_persisted_to_port);
  RUN_TEST(test_log_level_default_and_debug_setter);
  RUN_TEST(test_debug_logs_are_sanitized_for_http_failures);
  RUN_TEST(test_http_validation_code_and_success_queue_logs);
  RUN_TEST(test_http_credential_classification_and_invalid_level);
  RUN_TEST(test_enqueue_not_ready_is_logged_before_flush);
  RUN_TEST(test_unrecognized_http_response_is_not_logged);
  return UNITY_END();
}
