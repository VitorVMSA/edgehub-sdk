#include "ehc_port_host.h"

#include <stdio.h>
#include <string.h>

#define EHC_HOST_MAX_REPLIES 16
#define EHC_HOST_STORE_BLOB 4096

typedef struct {
  char match[96];
  int http_status;
  char body[2048];
  uint32_t retry_after_sec;
  int fail_transport;
} ehc_host_reply_t;

typedef struct {
  char key[96];
  uint8_t blob[EHC_HOST_STORE_BLOB];
  size_t len;
} ehc_host_store_slot_t;

typedef struct {
  uint64_t now_ms;
  ehc_host_http_capture_t captures[EHC_HOST_MAX_CAPTURES];
  int capture_count;
  ehc_host_reply_t replies[EHC_HOST_MAX_REPLIES];
  int reply_count;
  int default_status;
  char default_body[256];
  ehc_host_store_slot_t store[EHC_HOST_MAX_STORE];
  int store_count;
  int store_fail_write;
  char logs[EHC_HOST_MAX_LOGS][192];
  int log_count;
} ehc_host_state_t;

static ehc_host_state_t s_host;

void ehc_host_reset(void)
{
  memset(&s_host, 0, sizeof(s_host));
  s_host.default_status = 200;
  snprintf(s_host.default_body, sizeof(s_host.default_body), "{}");
}

void ehc_host_clock_set(uint64_t ms)
{
  s_host.now_ms = ms;
}

void ehc_host_clock_advance(uint64_t ms)
{
  s_host.now_ms += ms;
}

void ehc_test_clock_advance(uint64_t ms)
{
  ehc_host_clock_advance(ms);
}

uint64_t ehc_host_now(void)
{
  return s_host.now_ms;
}

void ehc_host_push_reply(const char *url_substr, int http_status, const char *body)
{
  ehc_host_push_reply_ex(url_substr, http_status, body, 0, 0);
}

void ehc_host_push_reply_ex(
  const char *url_substr, int http_status, const char *body, uint32_t retry_after_sec, int fail_transport)
{
  ehc_host_reply_t *r;
  if (s_host.reply_count >= EHC_HOST_MAX_REPLIES) {
    return;
  }
  r = &s_host.replies[s_host.reply_count++];
  memset(r, 0, sizeof(*r));
  if (url_substr) {
    strncpy(r->match, url_substr, sizeof(r->match) - 1);
  }
  r->http_status = http_status;
  strncpy(r->body, body ? body : "", sizeof(r->body) - 1);
  r->retry_after_sec = retry_after_sec;
  r->fail_transport = fail_transport;
}

static const ehc_host_reply_t *find_reply(const char *url)
{
  int i;
  for (i = 0; i < s_host.reply_count; i++) {
    if (!s_host.replies[i].match[0] || (url && strstr(url, s_host.replies[i].match))) {
      return &s_host.replies[i];
    }
  }
  return NULL;
}

int ehc_host_capture_count(void)
{
  return s_host.capture_count;
}

const ehc_host_http_capture_t *ehc_host_capture(int index)
{
  if (index < 0 || index >= s_host.capture_count) {
    return NULL;
  }
  return &s_host.captures[index];
}

const ehc_host_http_capture_t *ehc_host_find_url(const char *substr)
{
  int i;
  for (i = 0; i < s_host.capture_count; i++) {
    if (substr && strstr(s_host.captures[i].url, substr)) {
      return &s_host.captures[i];
    }
  }
  return NULL;
}

void ehc_host_clear_captures(void)
{
  memset(s_host.captures, 0, sizeof(s_host.captures));
  s_host.capture_count = 0;
}

void ehc_host_store_put(const char *key, const char *value)
{
  int i;
  ehc_host_store_slot_t *slot = NULL;
  size_t n;
  if (!key) {
    return;
  }
  value = value ? value : "";
  n = strlen(value) + 1;
  for (i = 0; i < s_host.store_count; i++) {
    if (strcmp(s_host.store[i].key, key) == 0) {
      slot = &s_host.store[i];
      break;
    }
  }
  if (!slot) {
    if (s_host.store_count >= EHC_HOST_MAX_STORE) {
      return;
    }
    slot = &s_host.store[s_host.store_count++];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->key, key, sizeof(slot->key) - 1);
  }
  if (n > sizeof(slot->blob)) {
    n = sizeof(slot->blob);
  }
  memcpy(slot->blob, value, n);
  slot->len = n;
}

int ehc_host_store_get(const char *key, char *out, size_t cap)
{
  int i;
  size_t n;
  if (!key || !out || cap == 0) {
    return 0;
  }
  for (i = 0; i < s_host.store_count; i++) {
    if (strcmp(s_host.store[i].key, key) == 0) {
      n = s_host.store[i].len;
      if (n >= cap) {
        n = cap - 1;
      }
      memcpy(out, s_host.store[i].blob, n);
      out[n] = '\0';
      if (n > 0 && out[n - 1] == '\0') {
        /* already NUL-terminated blob */
      }
      return 1;
    }
  }
  out[0] = '\0';
  return 0;
}

void ehc_host_set_store_fail_write(int fail)
{
  s_host.store_fail_write = fail ? 1 : 0;
}

int ehc_host_log_count(void)
{
  return s_host.log_count;
}

int ehc_host_log_count_containing(const char *substr)
{
  int i;
  int n = 0;
  for (i = 0; i < s_host.log_count; i++) {
    if (substr && strstr(s_host.logs[i], substr)) {
      n++;
    }
  }
  return n;
}

const char *ehc_host_log_at(int index)
{
  if (index < 0 || index >= s_host.log_count) {
    return "";
  }
  return s_host.logs[index];
}

static int host_http(
  void *ctx, const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
  uint32_t *retry_after_sec)
{
  const ehc_host_reply_t *reply;
  ehc_host_http_capture_t *cap;
  const char *out_body;
  int status;
  size_t n;
  (void)ctx;

  if (s_host.capture_count < EHC_HOST_MAX_CAPTURES) {
    cap = &s_host.captures[s_host.capture_count++];
    memset(cap, 0, sizeof(*cap));
    if (url) {
      strncpy(cap->url, url, sizeof(cap->url) - 1);
    }
    if (headers) {
      strncpy(cap->headers, headers, sizeof(cap->headers) - 1);
    }
    if (body && body_len < sizeof(cap->body)) {
      memcpy(cap->body, body, body_len);
      cap->body[body_len] = '\0';
      cap->body_len = body_len;
    }
  }

  reply = find_reply(url);
  if (reply && reply->fail_transport) {
    return 0;
  }
  status = reply ? reply->http_status : s_host.default_status;
  out_body = reply ? reply->body : s_host.default_body;
  n = strlen(out_body);
  if (resp && resp_cap > n) {
    memcpy(resp, out_body, n);
    resp[n] = '\0';
  } else if (resp && resp_cap > 0) {
    size_t c = resp_cap - 1;
    if (c > n) {
      c = n;
    }
    memcpy(resp, out_body, c);
    resp[c] = '\0';
    n = c;
  }
  if (resp_len) {
    *resp_len = n;
  }
  if (http_status) {
    *http_status = status;
  }
  if (retry_after_sec) {
    *retry_after_sec = (reply && status == 429) ? reply->retry_after_sec : 0;
  }
  return 1;
}

static int host_store_read(void *ctx, const char *key, uint8_t *buf, size_t cap, size_t *len)
{
  int i;
  (void)ctx;
  for (i = 0; i < s_host.store_count; i++) {
    if (strcmp(s_host.store[i].key, key) == 0) {
      size_t n = s_host.store[i].len;
      if (n > cap) {
        n = cap;
      }
      memcpy(buf, s_host.store[i].blob, n);
      if (len) {
        *len = n;
      }
      return 1;
    }
  }
  return 0;
}

static int host_store_write(void *ctx, const char *key, const uint8_t *buf, size_t len)
{
  int i;
  ehc_host_store_slot_t *slot = NULL;
  (void)ctx;
  if (s_host.store_fail_write) {
    return 0;
  }
  for (i = 0; i < s_host.store_count; i++) {
    if (strcmp(s_host.store[i].key, key) == 0) {
      slot = &s_host.store[i];
      break;
    }
  }
  if (!slot) {
    if (s_host.store_count >= EHC_HOST_MAX_STORE) {
      return 0;
    }
    slot = &s_host.store[s_host.store_count++];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->key, key, sizeof(slot->key) - 1);
  }
  if (len > sizeof(slot->blob)) {
    return 0;
  }
  memcpy(slot->blob, buf, len);
  slot->len = len;
  return 1;
}

static uint64_t host_now(void *ctx)
{
  (void)ctx;
  return s_host.now_ms;
}

static void host_log(void *ctx, ehc_log_level_t lvl, const char *msg)
{
  (void)ctx;
  (void)lvl;
  if (s_host.log_count < EHC_HOST_MAX_LOGS) {
    strncpy(s_host.logs[s_host.log_count], msg ? msg : "", sizeof(s_host.logs[0]) - 1);
    s_host.log_count++;
  }
}

void ehc_host_fill_port(ehc_port_t *port)
{
  if (!port) {
    return;
  }
  memset(port, 0, sizeof(*port));
  port->http_post = host_http;
  port->storage_read = host_store_read;
  port->storage_write = host_store_write;
  port->now_ms = host_now;
  port->log = host_log;
}
