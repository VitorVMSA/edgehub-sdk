#include "unity.h"
#include "edgehub_device.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_empty_object(void)
{
  char buf[64];
  eh_values_writer_t w;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&w));
  TEST_ASSERT_EQUAL_STRING("{}", buf);
}

static void test_one_and_several_siblings(void)
{
  char buf[128];
  eh_values_writer_t w;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_str(&w, "type", "telemetry"));
  TEST_ASSERT_EQUAL_STRING("{\"type\":\"telemetry\"", buf);
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_num(&w, "temp", 22.5));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_bool(&w, "ok", 1));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&w));
  TEST_ASSERT_EQUAL_STRING("{\"type\":\"telemetry\",\"temp\":22.5,\"ok\":true}", buf);
}

static void test_bool_false(void)
{
  char buf[32];
  eh_values_writer_t w;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_bool(&w, "ok", 0));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&w));
  TEST_ASSERT_EQUAL_STRING("{\"ok\":false}", buf);
}

static void test_key_escape(void)
{
  char buf[64];
  eh_values_writer_t w;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_str(&w, "a\"b", "x\\y"));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&w));
  TEST_ASSERT_EQUAL_STRING("{\"a\\\"b\":\"x\\\\y\"}", buf);
}

static void test_add_raw_object_not_string(void)
{
  char scratch[64];
  char buf[96];
  eh_values_writer_t child;
  eh_values_writer_t parent;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&child, scratch, sizeof(scratch)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_num(&child, "rpm", 1500));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&child));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&parent, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_raw(&parent, "motor", scratch));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&parent));
  TEST_ASSERT_EQUAL_STRING("{\"motor\":{\"rpm\":1500}}", buf);
  TEST_ASSERT(strstr(buf, "\"motor\":\"{") == NULL);
}

static void test_add_raw_null_rejected(void)
{
  char buf[32];
  eh_values_writer_t w;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_ERR_INVALID_ARG, eh_values_add_raw(&w, "k", NULL));
}

static void test_buffer_too_small_does_not_overflow(void)
{
  char buf[8];
  eh_values_writer_t w;
  memset(buf, 0x5A, sizeof(buf));
  TEST_ASSERT_EQUAL(EH_ERR_INVALID_ARG, eh_values_begin(&w, buf, 2));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_ERR_BUFFER_TOO_SMALL, eh_values_add_str(&w, "longish", "value"));
  TEST_ASSERT_EQUAL((int)(unsigned char)0x5A, (int)(unsigned char)buf[7]);
}

static void test_num_format_15g(void)
{
  char buf[96];
  eh_values_writer_t w;
  TEST_ASSERT_EQUAL(EH_OK, eh_values_begin(&w, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_add_num(&w, "n", 1e21));
  TEST_ASSERT_EQUAL(EH_OK, eh_values_end(&w));
  TEST_ASSERT(strstr(buf, "1e+") != NULL || strstr(buf, "1e+") != NULL || strstr(buf, "1e+21") != NULL || strstr(buf, "1e+021") != NULL || strstr(buf, "1e+21") != NULL);
  TEST_ASSERT(strstr(buf, "\"n\":") != NULL);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_empty_object);
  RUN_TEST(test_one_and_several_siblings);
  RUN_TEST(test_bool_false);
  RUN_TEST(test_key_escape);
  RUN_TEST(test_add_raw_object_not_string);
  RUN_TEST(test_add_raw_null_rejected);
  RUN_TEST(test_buffer_too_small_does_not_overflow);
  RUN_TEST(test_num_format_15g);
  return UNITY_END();
}
