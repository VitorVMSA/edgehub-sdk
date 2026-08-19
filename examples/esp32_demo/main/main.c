#include "ehc_port_esp32.h"
#include "ehc_user.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "ehc_demo";

static void fill_user(ehc_user_t *u)
{
  memset(u, 0, sizeof(*u));
  u->base_url = (ehc_identity_source_t)EHC_IDENT_LIT("https://example.edgehub.test");
  u->device_uid = (ehc_identity_source_t)EHC_IDENT_LIT("esp32-demo");
  u->group_key = (ehc_identity_source_t)EHC_IDENT_LIT("demo-group");
  u->product_code = (ehc_identity_source_t)EHC_IDENT_LIT("demo-product");
  u->checkin_keys = ehc_user_checkin_keys;
  u->checkin_key_count = ehc_user_checkin_keys_count;
  u->metrics = ehc_user_metrics;
  u->metric_count = ehc_user_metrics_count;
  u->commands = ehc_user_commands;
  u->command_count = ehc_user_commands_count;
  u->on_config_apply = ehc_user_on_config_apply;
}

void app_main(void)
{
  ehc_port_t port;
  ehc_user_t user;
  (void)nvs_flash_init();
  if (!ehc_port_esp32_init(&port)) {
    ESP_LOGE(TAG, "port init failed");
    return;
  }
  fill_user(&user);
  if (ehc_init(&port, &user) != EHC_OK) {
    ESP_LOGE(TAG, "ehc_init failed (%s)", ehc_not_ready_reason());
    return;
  }
  for (;;) {
    (void)ehc_tel_set_num("temperature", 22.5);
    (void)ehc_tick();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
