#include "ehc_port_embedded.h"

#include <string.h>

#define EHC_EMB_STORE_MAX 8
#define EHC_EMB_KEY_MAX 48
#define EHC_EMB_VAL_MAX 192

typedef struct {
  char key[EHC_EMB_KEY_MAX];
  uint8_t val[EHC_EMB_VAL_MAX];
  size_t len;
  uint8_t used;
} ehc_emb_slot_t;

static struct {
  ehc_emb_slot_t store[EHC_EMB_STORE_MAX];
  uint64_t now_ms;
} s_emb;

static int emb_http_post(
  void *ctx,
  const char *url,
  const char *headers,
  const uint8_t *body,
  size_t body_len,
  uint8_t *resp,
  size_t resp_cap,
  size_t *resp_len,
  int *http_status,
  uint32_t *retry_after_sec)
{
  (void)ctx;
  (void)url;
  (void)headers;
  (void)body;
  (void)body_len;
  (void)resp;
  (void)resp_cap;
  if (resp_len) {
    *resp_len = 0;
  }
  if (http_status) {
    *http_status = 0;
  }
  if (retry_after_sec) {
    *retry_after_sec = 0;
  }
  /* 0 = transporte falhou. Trocar por HTTPS; 4xx/5xx devem devolver nonzero. */
  return 0;
}

static int emb_store_read(void *ctx, const char *key, uint8_t *buf, size_t cap, size_t *len)
{
  size_t i;
  (void)ctx;
  if (len) {
    *len = 0;
  }
  if (!key || !buf) {
    return 0;
  }
  for (i = 0; i < EHC_EMB_STORE_MAX; i++) {
    if (s_emb.store[i].used && strcmp(s_emb.store[i].key, key) == 0) {
      size_t n = s_emb.store[i].len;
      if (n > cap) {
        n = cap;
      }
      memcpy(buf, s_emb.store[i].val, n);
      if (len) {
        *len = n;
      }
      return 1;
    }
  }
  return 0;
}

static int emb_store_write(void *ctx, const char *key, const uint8_t *buf, size_t len)
{
  size_t i;
  int empty = -1;
  (void)ctx;
  if (!key || !key[0]) {
    return 0;
  }
  if (len > EHC_EMB_VAL_MAX) {
    return 0;
  }
  for (i = 0; i < EHC_EMB_STORE_MAX; i++) {
    if (s_emb.store[i].used && strcmp(s_emb.store[i].key, key) == 0) {
      memcpy(s_emb.store[i].val, buf ? buf : (const uint8_t *)"", len);
      s_emb.store[i].len = len;
      return 1;
    }
    if (!s_emb.store[i].used && empty < 0) {
      empty = (int)i;
    }
  }
  if (empty < 0) {
    return 0;
  }
  strncpy(s_emb.store[empty].key, key, EHC_EMB_KEY_MAX - 1);
  s_emb.store[empty].key[EHC_EMB_KEY_MAX - 1] = '\0';
  memcpy(s_emb.store[empty].val, buf ? buf : (const uint8_t *)"", len);
  s_emb.store[empty].len = len;
  s_emb.store[empty].used = 1;
  return 1;
}

static uint64_t emb_now_ms(void *ctx)
{
  (void)ctx;
  s_emb.now_ms += 1000;
  return s_emb.now_ms;
}

static void emb_log(void *ctx, ehc_log_level_t lvl, const char *msg)
{
  (void)ctx;
  (void)lvl;
  if (msg) {
    (void)msg;
  }
}

int ehc_port_embedded_init(ehc_port_t *out)
{
  if (!out) {
    return 0;
  }
  memset(&s_emb, 0, sizeof(s_emb));
  memset(out, 0, sizeof(*out));
  out->http_post = emb_http_post;
  out->storage_read = emb_store_read;
  out->storage_write = emb_store_write;
  out->now_ms = emb_now_ms;
  out->log = emb_log;
  out->lock = NULL;
  out->unlock = NULL;
  out->ctx = &s_emb;
  return 1;
}

void ehc_port_embedded_deinit(void)
{
  memset(&s_emb, 0, sizeof(s_emb));
}
