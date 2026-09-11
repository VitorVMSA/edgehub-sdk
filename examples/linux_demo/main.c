#include "ehc_port_linux.h"
#include "ehc_user.h"
#include <stdio.h>
#include <string.h>

static void fill_user(ehc_user_t *u)
{
  memset(u, 0, sizeof(*u));
  u->base_url = (ehc_identity_source_t)EHC_IDENT_LIT("https://example.edgehub.test");
  u->device_uid = (ehc_identity_source_t)EHC_IDENT_LIT("demo-device");
  u->group_key = (ehc_identity_source_t)EHC_IDENT_LIT("demo-group");
  u->product_code = (ehc_identity_source_t)EHC_IDENT_LIT("demo-product");
  u->checkin_keys = ehc_user_checkin_keys;
  u->checkin_key_count = ehc_user_checkin_keys_count;
  u->metrics = ehc_user_metrics;
  u->metric_count = ehc_user_metrics_count;
  u->commands = ehc_user_commands;
  u->command_count = ehc_user_commands_count;
  u->on_config_apply = ehc_user_on_config_apply;
  u->on_download = ehc_user_on_download;
  u->telemetry_manual_only = 0;
  /* u.identity_pkcs8 = EHC_IDENT_STORE("identity_pkcs8"); auto-liga credential/issue */
}

int main(void)
{
  ehc_port_t port;
  ehc_user_t user;
  ehc_linux_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.storage_dir = "/tmp/edgehub-client-demo";
  cfg.timeout_ms = 12000;
  if (!ehc_port_linux_init(&port, &cfg)) {
    fprintf(stderr, "port init failed\n");
    return 1;
  }
  fill_user(&user);
  if (ehc_init(&port, &user) != EHC_OK) {
    fprintf(stderr, "ehc_init failed: %s\n", ehc_not_ready_reason());
    return 1;
  }
  printf("readiness=%s\n", ehc_readiness_name(ehc_get_readiness()));
  (void)ehc_tel_set_num("temperature", 22.5);
  (void)ehc_tel_set_num_in("motor", "rpm", 0);
  (void)ehc_tick();
  ehc_port_linux_deinit();
  return 0;
}
