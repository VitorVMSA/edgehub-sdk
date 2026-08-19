#include "ehc_test_support.h"
#include "edgehub_device.h"

static double s_uptime = 42;
static int get_uptime(double *out)
{
  if (out) {
    *out = s_uptime;
  }
  return 1;
}

static const ehc_metric_t s_metrics[] = {
  EHC_METRIC_NUM("temperature"),
  EHC_METRIC_OBJECT("motor"),
  EHC_METRIC_NUM_IN("motor", "rpm"),
  EHC_METRIC_BOOL_IN("motor", "running"),
  EHC_METRIC_STR("mode"),
  EHC_METRIC_NUM_GET("uptimeSec", get_uptime),
  EHC_METRIC_NUM_STALE("always"),
  EHC_METRIC_NUM("never"),
};

static const ehc_metric_t s_too_deep[] = {
  EHC_METRIC_OBJECT("motor"),
  EHC_METRIC_OBJECT_IN("motor", "driver"),
  EHC_METRIC_NUM_IN("driver", "temperature"),
};

static const ehc_metric_t s_dup_obj[] = {
  EHC_METRIC_OBJECT("motor"),
  EHC_METRIC_OBJECT("motor"),
};

static const ehc_metric_t s_many_str[] = {
  EHC_METRIC_STR("s0"),
  EHC_METRIC_STR("s1"),
  EHC_METRIC_STR("s2"),
  EHC_METRIC_STR("s3"),
  EHC_METRIC_STR("s4"),
  EHC_METRIC_STR("s5"),
  EHC_METRIC_STR("s6"),
  EHC_METRIC_STR("s7"),
  EHC_METRIC_STR("s8"),
};

void setUp(void)
{
  ehc_test_begin();
}

void tearDown(void)
{
  ehc_shutdown();
}

static void init_full_metrics(const ehc_metric_t *m, size_t n)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = m;
  u.metric_count = n;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_test_init(&u));
  ehc_test_become_active();
}

static const char *values_json(void)
{
  const ehc_host_http_capture_t *cap = ehc_host_find_url("/device/telemetry");
  const char *p;
  TEST_ASSERT_NOT_NULL(cap);
  p = strstr(cap->body, "\"values\":");
  TEST_ASSERT_NOT_NULL(p);
  return p;
}

static void test_nested_json(void)
{
  const char *v;
  init_full_metrics(s_metrics, 8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 22.5));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num_in("motor", "rpm", 1500));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_bool_in("motor", "running", 1));
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_str("mode", "auto"));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  v = values_json();
  TEST_ASSERT(strstr(v, "\"temperature\":22.5") != NULL);
  TEST_ASSERT(strstr(v, "\"motor\":{") != NULL);
  TEST_ASSERT(strstr(v, "\"rpm\":1500") != NULL);
  TEST_ASSERT(strstr(v, "\"running\":true") != NULL);
  TEST_ASSERT(strstr(v, "\"mode\":\"auto\"") != NULL);
  TEST_ASSERT(strstr(v, "\"uptimeSec\":42") != NULL);
}

static void test_omit_empty_object_and_unwritten(void)
{
  const char *v;
  init_full_metrics(s_metrics, 8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  v = values_json();
  TEST_ASSERT(strstr(v, "\"motor\"") == NULL);
  TEST_ASSERT(strstr(v, "\"never\"") == NULL);
  TEST_ASSERT(strstr(v, "\"always\":0") != NULL);
}

static void test_getter_beats_push(void)
{
  const char *v;
  init_full_metrics(s_metrics, 8);
  s_uptime = 99;
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("uptimeSec", 1));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  v = values_json();
  TEST_ASSERT(strstr(v, "\"uptimeSec\":99") != NULL);
}

static void test_path_setter(void)
{
  const char *v;
  init_full_metrics(s_metrics, 8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num_at("motor.rpm", 12));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  v = values_json();
  TEST_ASSERT(strstr(v, "\"rpm\":12") != NULL);
}

static void test_numeric_format_matches_sdk(void)
{
  char expect[64];
  char buf[128];
  eh_values_writer_t w;
  const char *v;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_num(&w, "x", 1e21));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&w));
  {
    const char *p = strstr(buf, "\"x\":");
    TEST_ASSERT_NOT_NULL(p);
    strncpy(expect, p + 4, sizeof(expect) - 1);
    expect[sizeof(expect) - 1] = '\0';
    {
      char *q = strchr(expect, '}');
      if (q) {
        *q = '\0';
      }
    }
  }
  init_full_metrics(s_metrics, 8);
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_set_num("temperature", 1e21));
  ehc_host_clear_captures();
  TEST_ASSERT_EQUAL(EHC_OK, ehc_tel_publish());
  v = values_json();
  TEST_ASSERT(strstr(v, expect) != NULL);
}

static void test_depth_rejected(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_too_deep;
  u.metric_count = 3;
  TEST_ASSERT_EQUAL(EHC_ECONFIG, ehc_test_init(&u));
}

static void test_duplicate_object_rejected(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_dup_obj;
  u.metric_count = 2;
  TEST_ASSERT_EQUAL(EHC_ECONFIG, ehc_test_init(&u));
}

static void test_string_pool_exhausted(void)
{
  ehc_user_t u;
  ehc_test_fill_user_full(&u);
  u.metrics = s_many_str;
  u.metric_count = 9;
  TEST_ASSERT_EQUAL(EHC_ECONFIG, ehc_test_init(&u));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_nested_json);
  RUN_TEST(test_omit_empty_object_and_unwritten);
  RUN_TEST(test_getter_beats_push);
  RUN_TEST(test_path_setter);
  RUN_TEST(test_numeric_format_matches_sdk);
  RUN_TEST(test_depth_rejected);
  RUN_TEST(test_duplicate_object_rejected);
  RUN_TEST(test_string_pool_exhausted);
  return UNITY_END();
}
