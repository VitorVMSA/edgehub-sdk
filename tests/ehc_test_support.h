#ifndef EHC_TEST_SUPPORT_H
#define EHC_TEST_SUPPORT_H

#include "unity.h"
#include "ehc_port_host.h"
#include "edgehub_client/ehc.h"
#include <string.h>

static void ehc_test_begin(void)
{
  ehc_shutdown();
  ehc_host_reset();
}

static void ehc_test_fill_user(ehc_user_t *u)
{
  memset(u, 0, sizeof(*u));
  u->base_url = (ehc_identity_source_t)EHC_IDENT_LIT("https://example.edgehub.test");
  u->device_uid = (ehc_identity_source_t)EHC_IDENT_LIT("dev-1");
  u->telemetry_manual_only = 1;
}

static void ehc_test_fill_user_full(ehc_user_t *u)
{
  ehc_test_fill_user(u);
  u->group_key = (ehc_identity_source_t)EHC_IDENT_LIT("gk");
  u->product_code = (ehc_identity_source_t)EHC_IDENT_LIT("pc");
}

static ehc_status_t ehc_test_init(const ehc_user_t *user)
{
  ehc_port_t port;
  ehc_host_fill_port(&port);
  return ehc_init(&port, user);
}

static void ehc_test_become_active(void)
{
  ehc_host_store_put("ehc.hmac.secret", "test-secret");
  ehc_host_push_reply("/device/checkin", 200, "{\"provisioningStatus\":\"active\"}");
  TEST_ASSERT_EQUAL(EHC_OK, ehc_force_checkin());
  TEST_ASSERT_EQUAL(EHC_ACTIVE, ehc_get_readiness());
}

#endif
