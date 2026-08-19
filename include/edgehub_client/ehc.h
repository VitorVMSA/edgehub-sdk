#ifndef EHC_H
#define EHC_H

#include "ehc_types.h"
#include "ehc_port.h"
#include "ehc_checkin.h"
#include "ehc_telemetry.h"
#include "ehc_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  ehc_identity_source_t base_url;
  ehc_identity_source_t tenant_slug;
  ehc_identity_source_t device_uid;
  ehc_identity_source_t group_key;
  ehc_identity_source_t product_code;
  ehc_identity_source_t identity_pkcs8;
  const ehc_checkin_key_t *checkin_keys;
  size_t checkin_key_count;
  const ehc_metric_t *metrics;
  size_t metric_count;
  const ehc_command_t *commands;
  size_t command_count;
  int (*on_config_apply)(
    const char *asset_key,
    const char *version,
    const char *source,
    const char *payload,
    void *ctx);
  void *user_ctx;
  uint8_t telemetry_manual_only;
  uint8_t enable_identity_credential_issue;
} ehc_user_t;

ehc_status_t ehc_init(const ehc_port_t *port, const ehc_user_t *user);
void ehc_shutdown(void);
int ehc_is_initialized(void);

ehc_status_t ehc_tick(void);
ehc_status_t ehc_force_checkin(void);

ehc_readiness_t ehc_get_readiness(void);
const char *ehc_readiness_name(ehc_readiness_t r);
const char *ehc_not_ready_reason(void);

#define EHC_LIB_VERSION "0.1.0"

#ifdef __cplusplus
}
#endif

#endif
