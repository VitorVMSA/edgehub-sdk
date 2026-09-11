#include "unity.h"
#include "eh_test_harness.h"

static eh_test_ctx_t g_ctx;
static eh_client_t g_cli;

void setUp(void)
{
  eh_test_ctx_reset(&g_ctx);
}

void tearDown(void) {}

static int store_has_key(const char *key)
{
  int i;
  for (i = 0; i < g_ctx.store_count; i++) {
    if (strcmp(g_ctx.store[i].key, key) == 0) {
      return 1;
    }
  }
  return 0;
}

static void init_cli(uint8_t batch_max)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_fill_cfg(&cfg);
  cfg.telemetry_batch_max = batch_max;
  cfg.telemetry_queue_capacity = 4;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.store_read = eh_test_store_read;
  cb.store_write = eh_test_store_write;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
}

static void test_legacy_single_body(void)
{
  init_cli(1);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_telemetry_ts(&g_cli, "2026-01-01T00:00:00.000Z"));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 0));
  TEST_ASSERT(strstr(g_ctx.captures[0].body, "\"readings\"") == NULL);
  TEST_ASSERT(strstr(g_ctx.captures[0].body, "\"ts\":\"2026-01-01T00:00:00.000Z\"") != NULL);
}

static void test_ts_from_now_ms(void)
{
  init_cli(1);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_force_telemetry(&g_cli, 100));
  TEST_ASSERT(strstr(g_ctx.captures[0].body, "\"ts\":\"1970-01-01T00:00:00.100Z\"") != NULL);
}

static void test_batch_body(void)
{
  init_cli(2);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:00.000Z", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:01.000Z", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_flush_telemetry(&g_cli, 0));
  TEST_ASSERT(strstr(g_ctx.captures[0].body, "\"readings\":[") != NULL);
  TEST_ASSERT_EQUAL(0, eh_client_telemetry_queue_count(&g_cli));
}

static void test_offline_keeps_queue(void)
{
  init_cli(2);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:00.000Z", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:01.000Z", 0));
  g_ctx.force_offline = 1;
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_flush_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(2, eh_client_telemetry_queue_count(&g_cli));
}

static void test_queue_drop_oldest(void)
{
  init_cli(1);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "t0", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "t1", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "t2", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "t3", 0));
  TEST_ASSERT_EQUAL(4, eh_client_telemetry_queue_count(&g_cli));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "t4", 0));
  TEST_ASSERT_EQUAL(4, eh_client_telemetry_queue_count(&g_cli));
}

static void test_rejected_index_drops_only_that_slot(void)
{
  init_cli(2);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:00.000Z", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:01.000Z", 0));
  eh_test_push_reply(&g_ctx, "/device/telemetry", 401, "{\"code\":\"TELEMETRY_INVALID_TS\",\"rejectedIndex\":0}");
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_flush_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(1, eh_client_telemetry_queue_count(&g_cli));
}

static void test_queue_persists_and_reloads(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  init_cli(2);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:00.000Z", 0));
  TEST_ASSERT(g_ctx.store_count > 0);
  TEST_ASSERT_EQUAL(1, store_has_key("eh.tel.q"));

  eh_test_fill_cfg(&cfg);
  cfg.telemetry_batch_max = 2;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.store_read = eh_test_store_read;
  cb.store_write = eh_test_store_write;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(1, eh_client_telemetry_queue_count(&g_cli));
}

static void test_queue_clear_after_offline_flush(void)
{
  init_cli(2);
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:00.000Z", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:01.000Z", 0));
  g_ctx.force_offline = 1;
  TEST_ASSERT_NOT_EQUAL(EH_OK, eh_client_flush_telemetry(&g_cli, 0));
  TEST_ASSERT_EQUAL(2, eh_client_telemetry_queue_count(&g_cli));
  eh_client_telemetry_queue_clear(&g_cli);
  TEST_ASSERT_EQUAL(0, eh_client_telemetry_queue_count(&g_cli));
}

static void test_persist_zero_skips_store(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  eh_test_fill_cfg(&cfg);
  cfg.telemetry_batch_max = 2;
  cfg.telemetry_queue_capacity = 4;
  cfg.telemetry_queue_persist = 0;
  memset(&cb, 0, sizeof(cb));
  cb.http_post = eh_test_http_post;
  cb.store_read = eh_test_store_read;
  cb.store_write = eh_test_store_write;
  cb.telemetry_collect = eh_test_telemetry_collect;
  cb.user_ctx = &g_ctx;
  TEST_ASSERT_EQUAL(EH_OK, eh_client_init(&g_cli, &cfg, &cb, 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_set_secret(&g_cli, "test-secret", NULL));
  TEST_ASSERT_EQUAL(EH_OK, eh_client_enqueue_reading(&g_cli, "2024-06-01T12:00:00.000Z", 0));
  TEST_ASSERT_EQUAL(1, eh_client_telemetry_queue_count(&g_cli));
  TEST_ASSERT_EQUAL(0, store_has_key("eh.tel.q"));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_legacy_single_body);
  RUN_TEST(test_ts_from_now_ms);
  RUN_TEST(test_batch_body);
  RUN_TEST(test_offline_keeps_queue);
  RUN_TEST(test_queue_drop_oldest);
  RUN_TEST(test_rejected_index_drops_only_that_slot);
  RUN_TEST(test_queue_persists_and_reloads);
  RUN_TEST(test_queue_clear_after_offline_flush);
  RUN_TEST(test_persist_zero_skips_store);
  return UNITY_END();
}
