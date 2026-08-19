#ifndef EHC_PORT_ESP32_H
#define EHC_PORT_ESP32_H

#include "edgehub_client/ehc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * HTTP via esp_http_client + crt bundle. Storage via NVS namespace "ehc".
 *
 * NVS I/O must run on a task whose stack is in internal RAM (not PSRAM).
 * Compile with -DEHC_PORT_ESP32_STORAGE_WORKER=1 to bounce storage onto
 * an internal-RAM worker. The EdgeHub client task may then live in PSRAM.
 */
int ehc_port_esp32_init(ehc_port_t *out);
void ehc_port_esp32_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
