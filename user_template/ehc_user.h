#ifndef EHC_USER_H
#define EHC_USER_H

#include "edgehub_client/ehc.h"

/* memset(&user, 0) = master actual (check-in + telemetria + comandos).
 * Slave: user.disable_telemetry = 1; metric_count = 0.
 * PKCS#8: u.identity_pkcs8 = EHC_IDENT_STORE("..."); auto-liga credential/issue.
 * Slave com comandos MDM: poll_commands = 1 (POST telemetria vazia; conta telemetry_events). */

#ifdef __cplusplus
extern "C" {
#endif

extern const ehc_metric_t ehc_user_metrics[];
extern const size_t ehc_user_metrics_count;
extern const ehc_command_t ehc_user_commands[];
extern const size_t ehc_user_commands_count;
extern const ehc_checkin_key_t ehc_user_checkin_keys[];
extern const size_t ehc_user_checkin_keys_count;
int ehc_user_on_config_apply(
  const char *asset_key, const char *version, const char *source, const char *payload, void *ctx);
void ehc_user_on_download(
  const char *asset_key, const char *version, const char *url, const char *sum);

#ifdef __cplusplus
}
#endif

#endif
