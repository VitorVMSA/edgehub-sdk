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
  ehc_identity_source_t identity_pkcs8; /* PKCS#8 Ed25519; se resolver, auto-liga credential/issue */
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
  /** Check-in `downloads`; NULL = só log. */
  void (*on_download)(
    const char *asset_key,
    const char *version,
    const char *url,
    const char *sum);
  void *user_ctx;
  uint8_t telemetry_manual_only;
  /** 0 = `EHC_TELEMETRY_BATCH_MAX`. Valor > 1 envia `readings[]`. */
  uint8_t telemetry_batch_max;
  uint8_t enable_identity_credential_issue;
  uint8_t disable_telemetry; /* 1 = sem POST telemetria; tabelas podem estar vazias */
  uint8_t disable_commands;
  uint8_t disable_checkin;
  /**
   * Slave: POST periódico `{values:{}}` no ingest de telemetria para puxar `pendingCommands`.
   * memset 0 = off. Só tem efeito com `disable_telemetry=1` e `disable_commands=0`.
   * Conta crédito `telemetry_events`.
   */
  uint8_t poll_commands;
} ehc_user_t;

typedef struct {
  uint8_t provisioning; /* eh_provisioning_status_t */
  uint64_t next_checkin_at_ms;
  uint64_t backoff_until_ms;
  uint8_t pending_command_count;
} ehc_device_status_t;

ehc_status_t ehc_init(const ehc_port_t *port, const ehc_user_t *user);
void ehc_shutdown(void);
int ehc_is_initialized(void);

ehc_status_t ehc_tick(void);
ehc_status_t ehc_force_checkin(void);

/** Intervalo de check-in (ms). `0` = default do device. Não chamar de `on_config_apply`. */
ehc_status_t ehc_set_checkin_interval(uint32_t ms);
ehc_status_t ehc_set_telemetry_batch_max(uint8_t n);
/** No-op útil se `telemetry_manual_only`. Não chamar de `on_config_apply`. */
ehc_status_t ehc_set_telemetry_interval(uint32_t ms);
ehc_status_t ehc_get_device_status(ehc_device_status_t *out);

ehc_readiness_t ehc_get_readiness(void);
const char *ehc_readiness_name(ehc_readiness_t r);
const char *ehc_not_ready_reason(void);

#define EHC_LIB_VERSION "0.3.0"

#ifdef __cplusplus
}
#endif

#endif
