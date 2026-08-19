#ifndef EHC_PORT_LINUX_H
#define EHC_PORT_LINUX_H

#include "edgehub_client/ehc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *storage_dir;
  uint32_t timeout_ms;
} ehc_linux_cfg_t;

int ehc_port_linux_init(ehc_port_t *out, const ehc_linux_cfg_t *cfg);
void ehc_port_linux_deinit(void);
int ehc_thread_start(uint32_t period_ms);
void ehc_thread_stop(void);

#ifdef __cplusplus
}
#endif

#endif
