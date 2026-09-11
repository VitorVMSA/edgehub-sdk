#include "unity.h"
#include "edgehub_device.h"

void setUp(void) {}
void tearDown(void) {}

static void test_abi_check_ok(void)
{
  TEST_ASSERT_EQUAL(EH_OK, eh_lib_abi_check());
}

static void test_version_string(void)
{
  TEST_ASSERT_EQUAL_STRING(
    EH_LIB_VERSION_STRINGIFY(EH_LIB_VERSION_MAJOR) "." EH_LIB_VERSION_STRINGIFY(EH_LIB_VERSION_MINOR) "." EH_LIB_VERSION_STRINGIFY(EH_LIB_VERSION_PATCH),
    eh_lib_version_string());
  TEST_ASSERT_EQUAL(EH_LIB_ABI_VERSION, (int)eh_lib_abi_version());
}

static void test_struct_sizes(void)
{
  TEST_ASSERT_EQUAL((int)sizeof(eh_config_t), (int)eh_struct_size_config());
  TEST_ASSERT_EQUAL((int)sizeof(eh_client_t), (int)eh_struct_size_client());
  TEST_ASSERT_EQUAL((int)sizeof(eh_callbacks_t), (int)eh_struct_size_callbacks());
  TEST_ASSERT_EQUAL((int)sizeof(eh_client_status_t), (int)eh_struct_size_status());
}

static void test_abi_check_impl_rejects_size_mismatch(void)
{
  TEST_ASSERT_EQUAL(
    EH_ERR_INVALID_ARG,
    eh_lib_abi_check_impl(
      sizeof(eh_config_t),
      sizeof(eh_client_t) - 1,
      sizeof(eh_callbacks_t),
      sizeof(eh_client_status_t),
      EH_LIB_ABI_VERSION));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_abi_check_ok);
  RUN_TEST(test_version_string);
  RUN_TEST(test_struct_sizes);
  RUN_TEST(test_abi_check_impl_rejects_size_mismatch);
  return UNITY_END();
}
