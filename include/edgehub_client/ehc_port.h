#ifndef EHC_PORT_H
#define EHC_PORT_H

#include "ehc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int (*http_post)(
    void *ctx,
    const char *url,
    const char *headers,
    const uint8_t *body,
    size_t body_len,
    uint8_t *resp,
    size_t resp_cap,
    size_t *resp_len,
    int *http_status,
    uint32_t *retry_after_sec);
  int (*storage_read)(void *ctx, const char *key, uint8_t *buf, size_t cap, size_t *len);
  int (*storage_write)(void *ctx, const char *key, const uint8_t *buf, size_t len);
  uint64_t (*now_ms)(void *ctx);
  void (*log)(void *ctx, ehc_log_level_t lvl, const char *msg);
  void (*lock)(void *ctx);
  void (*unlock)(void *ctx);
  void *ctx;
} ehc_port_t;

#ifdef __cplusplus
}
#endif

#endif
