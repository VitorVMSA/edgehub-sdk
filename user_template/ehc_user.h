#ifndef EHC_USER_H
#define EHC_USER_H

#include "edgehub_client/ehc.h"

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

#ifdef __cplusplus
}
#endif

#endif
