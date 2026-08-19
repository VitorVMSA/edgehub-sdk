#ifndef EHC_PORT_HOST_H
#define EHC_PORT_HOST_H

#include "edgehub_client/ehc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EHC_HOST_MAX_CAPTURES 24
#define EHC_HOST_MAX_BODY 8192
#define EHC_HOST_MAX_HEADERS 768
#define EHC_HOST_MAX_URL 384
#define EHC_HOST_MAX_STORE 32
#define EHC_HOST_MAX_LOGS 32

typedef struct {
  char url[EHC_HOST_MAX_URL];
  char headers[EHC_HOST_MAX_HEADERS];
  char body[EHC_HOST_MAX_BODY];
  size_t body_len;
} ehc_host_http_capture_t;

void ehc_host_reset(void);
void ehc_host_fill_port(ehc_port_t *port);

void ehc_host_clock_set(uint64_t ms);
void ehc_host_clock_advance(uint64_t ms);
void ehc_test_clock_advance(uint64_t ms);
uint64_t ehc_host_now(void);

void ehc_host_push_reply(const char *url_substr, int http_status, const char *body);
void ehc_host_push_reply_ex(
  const char *url_substr, int http_status, const char *body, uint32_t retry_after_sec, int fail_transport);

int ehc_host_capture_count(void);
const ehc_host_http_capture_t *ehc_host_capture(int index);
const ehc_host_http_capture_t *ehc_host_find_url(const char *substr);
void ehc_host_clear_captures(void);

void ehc_host_store_put(const char *key, const char *value);
int ehc_host_store_get(const char *key, char *out, size_t cap);
void ehc_host_set_store_fail_write(int fail);

int ehc_host_log_count(void);
int ehc_host_log_count_containing(const char *substr);
const char *ehc_host_log_at(int index);

#ifdef __cplusplus
}
#endif

#endif
