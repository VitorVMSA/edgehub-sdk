#include "ehc_port_esp32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

/** Abaixo disto o relógio ainda é boot/1970; o ingest rejeita o `ts`. */
#define EHC_UNIX_MIN_SEC 1600000000L

#ifndef EHC_PORT_ESP32_STORAGE_WORKER
#define EHC_PORT_ESP32_STORAGE_WORKER 0
#endif

static const char *TAG = "ehc_port_esp32";
#define EHC_NVS_NS "ehc"

typedef struct {
  uint8_t *buf;
  size_t cap;
  size_t len;
  uint32_t retry_after_sec;
} ehc_esp_resp_t;

typedef struct {
  int is_write;
  char key[96];
  uint8_t *buf;
  size_t cap;
  size_t len;
  int result;
  SemaphoreHandle_t done;
} ehc_nvs_job_t;

static SemaphoreHandle_t s_mu;
static volatile int s_last_http;
static char s_last_snip[160];
static char s_last_op[16];
static char s_last_cmd[48];
#if EHC_PORT_ESP32_STORAGE_WORKER
static QueueHandle_t s_nvs_q;
static TaskHandle_t s_nvs_task;
#endif

static int clock_is_unix(void)
{
  struct timeval tv;
  return gettimeofday(&tv, NULL) == 0 && tv.tv_sec >= EHC_UNIX_MIN_SEC;
}

static void maybe_set_time_from_http_date(const char *value)
{
  static const char *k_mon[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  struct tm tm;
  struct timeval tv;
  time_t t;
  int d = 0;
  int year = 0;
  int hh = 0;
  int mm = 0;
  int ss = 0;
  int month = -1;
  int i;
  char mon[8];
  const char *p;

  if (!value || clock_is_unix()) {
    return;
  }
  p = strchr(value, ',');
  p = p ? p + 1 : value;
  memset(&tm, 0, sizeof(tm));
  memset(mon, 0, sizeof(mon));
  if (sscanf(p, " %d %3s %d %d:%d:%d", &d, mon, &year, &hh, &mm, &ss) != 6) {
    return;
  }
  for (i = 0; i < 12; i++) {
    if (strncmp(mon, k_mon[i], 3) == 0) {
      month = i;
      break;
    }
  }
  if (month < 0 || d < 1 || d > 31 || year < 2020) {
    return;
  }
  tm.tm_mday = d;
  tm.tm_mon = month;
  tm.tm_year = year - 1900;
  tm.tm_hour = hh;
  tm.tm_min = mm;
  tm.tm_sec = ss;
  tm.tm_isdst = 0;
  t = mktime(&tm);
  if (t < EHC_UNIX_MIN_SEC) {
    return;
  }
  tv.tv_sec = t;
  tv.tv_usec = 0;
  if (settimeofday(&tv, NULL) == 0) {
    ESP_LOGI(TAG, "clock from HTTP Date unix=%ld", (long)t);
  }
}

static uint32_t fnv1a(const char *s)
{
  uint32_t h = 2166136261u;
  while (s && *s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

static void nvs_key_of(const char *key, char *out, size_t cap)
{
  snprintf(out, cap, "k%08x", (unsigned)fnv1a(key ? key : ""));
}

static void note_http_op(const char *url)
{
  const char *op = "http";
  if (!url || !url[0]) {
    s_last_op[0] = '\0';
    return;
  }
  if (strstr(url, "command-executions")) {
    op = "executed";
  } else if (strstr(url, "/checkin")) {
    op = "checkin";
  } else if (strstr(url, "/telemetry")) {
    op = "telemetry";
  } else if (strstr(url, "credential") || strstr(url, "/issue")) {
    op = "issue";
  }
  strncpy(s_last_op, op, sizeof(s_last_op) - 1);
  s_last_op[sizeof(s_last_op) - 1] = '\0';
}

static esp_err_t http_event(esp_http_client_event_t *evt)
{
  ehc_esp_resp_t *ctx = (ehc_esp_resp_t *)evt->user_data;
  if (evt->event_id == HTTP_EVENT_ON_HEADER && ctx && evt->header_key && evt->header_value) {
    if (strcasecmp(evt->header_key, "Retry-After") == 0) {
      ctx->retry_after_sec = (uint32_t)atoi(evt->header_value);
    } else if (strcasecmp(evt->header_key, "Date") == 0) {
      maybe_set_time_from_http_date(evt->header_value);
    }
  }
  return ESP_OK;
}

static void apply_headers(esp_http_client_handle_t client, const char *headers)
{
  const char *p = headers;
  (void)esp_http_client_set_header(client, "Content-Type", "application/json");
  if (!p) {
    return;
  }
  while (*p) {
    const char *nl = strstr(p, "\r\n");
    const char *colon;
    char name[64];
    char value[192];
    size_t nlen;
    size_t vlen;
    const char *line_end = nl ? nl : p + strlen(p);
    colon = memchr(p, ':', (size_t)(line_end - p));
    if (colon) {
      nlen = (size_t)(colon - p);
      if (nlen >= sizeof(name)) {
        nlen = sizeof(name) - 1;
      }
      memcpy(name, p, nlen);
      name[nlen] = '\0';
      colon++;
      while (*colon == ' ') {
        colon++;
      }
      vlen = (size_t)(line_end - colon);
      if (vlen >= sizeof(value)) {
        vlen = sizeof(value) - 1;
      }
      memcpy(value, colon, vlen);
      value[vlen] = '\0';
      if (name[0]) {
        (void)esp_http_client_set_header(client, name, value);
      }
    }
    if (!nl) {
      break;
    }
    p = nl + 2;
  }
}

static int esp_http_once(
  const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  ehc_esp_resp_t *rctx, int *http_status)
{
  esp_http_client_config_t cfg;
  esp_http_client_handle_t http;
  esp_err_t err;
  int written;
  int n;

  rctx->len = 0;
  rctx->retry_after_sec = 0;
  if (rctx->buf && rctx->cap) {
    rctx->buf[0] = '\0';
  }
  *http_status = 0;

  memset(&cfg, 0, sizeof(cfg));
  cfg.url = url;
  cfg.method = HTTP_METHOD_POST;
  cfg.timeout_ms = 12000;
  cfg.event_handler = http_event;
  cfg.user_data = rctx;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.keep_alive_enable = false;
  cfg.max_authorization_retries = -1;
  http = esp_http_client_init(&cfg);
  if (!http) {
    return 0;
  }
  apply_headers(http, headers);
  (void)esp_http_client_set_method(http, HTTP_METHOD_POST);

  err = esp_http_client_open(http, (int)body_len);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "HTTP open failed: %s url=%s", esp_err_to_name(err), url);
    esp_http_client_cleanup(http);
    return 0;
  }
  written = esp_http_client_write(http, body ? (const char *)body : "", (int)body_len);
  if (written < 0 || (body_len > 0 && (size_t)written != body_len)) {
    ESP_LOGW(TAG, "HTTP write failed url=%s", url);
    esp_http_client_close(http);
    esp_http_client_cleanup(http);
    return 0;
  }
  if (esp_http_client_fetch_headers(http) < 0) {
    ESP_LOGW(TAG, "HTTP headers failed url=%s", url);
    esp_http_client_close(http);
    esp_http_client_cleanup(http);
    return 0;
  }
  *http_status = esp_http_client_get_status_code(http);
  while (rctx->buf && rctx->len + 1 < rctx->cap) {
    n = esp_http_client_read(http, (char *)rctx->buf + rctx->len, rctx->cap - 1 - rctx->len);
    if (n <= 0) {
      break;
    }
    rctx->len += (size_t)n;
    rctx->buf[rctx->len] = '\0';
  }
  esp_http_client_close(http);
  esp_http_client_cleanup(http);
  return 1;
}

static int esp_http(
  void *ctx, const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
  uint32_t *retry_after_sec)
{
  ehc_esp_resp_t rctx;
  int status = 0;
  int ok;
  (void)ctx;

  if (resp_len) {
    *resp_len = 0;
  }
  if (http_status) {
    *http_status = 0;
  }
  if (retry_after_sec) {
    *retry_after_sec = 0;
  }
  if (!url || !resp || resp_cap == 0 || !http_status) {
    return 0;
  }
  note_http_op(url);

  rctx.buf = resp;
  rctx.cap = resp_cap;
  rctx.len = 0;
  rctx.retry_after_sec = 0;
  ok = esp_http_once(url, headers, body, body_len, &rctx, &status);
  if (!ok) {
    ESP_LOGW(TAG, "HTTP transport fail, retry once");
    ok = esp_http_once(url, headers, body, body_len, &rctx, &status);
  }
  if (!ok) {
    s_last_http = 0;
    s_last_snip[0] = '\0';
    return 0;
  }
  *http_status = status;
  if (resp_len) {
    *resp_len = rctx.len;
  }
  if (retry_after_sec) {
    *retry_after_sec = rctx.retry_after_sec;
  }
  s_last_http = status;
  {
    size_t n = rctx.len < sizeof(s_last_snip) - 1 ? rctx.len : sizeof(s_last_snip) - 1;
    if (n > 0 && rctx.buf) {
      memcpy(s_last_snip, rctx.buf, n);
      s_last_snip[n] = '\0';
    } else {
      s_last_snip[0] = '\0';
    }
  }
  if (status >= 400) {
    ESP_LOGW(TAG, "http=%d %s", status, s_last_snip);
  }
  return 1;
}

int ehc_port_esp32_last_http_status(void)
{
  return s_last_http;
}

void ehc_port_esp32_last_http_snip(char *out, size_t cap)
{
  if (!out || cap == 0) {
    return;
  }
  strncpy(out, s_last_snip, cap - 1);
  out[cap - 1] = '\0';
}

void ehc_port_esp32_last_http_op(char *out, size_t cap)
{
  if (!out || cap == 0) {
    return;
  }
  strncpy(out, s_last_op, cap - 1);
  out[cap - 1] = '\0';
}

void ehc_port_esp32_last_cmd(char *out, size_t cap)
{
  if (!out || cap == 0) {
    return;
  }
  strncpy(out, s_last_cmd, cap - 1);
  out[cap - 1] = '\0';
}

static int nvs_read_impl(const char *key, uint8_t *buf, size_t cap, size_t *len)
{
  nvs_handle_t h;
  char nk[16];
  size_t n = cap;
  esp_err_t err;
  nvs_key_of(key, nk, sizeof(nk));
  err = nvs_open(EHC_NVS_NS, NVS_READONLY, &h);
  if (err != ESP_OK) {
    return 0;
  }
  err = nvs_get_blob(h, nk, buf, &n);
  nvs_close(h);
  if (err != ESP_OK) {
    return 0;
  }
  if (len) {
    *len = n;
  }
  return 1;
}

static int nvs_write_impl(const char *key, const uint8_t *buf, size_t len)
{
  nvs_handle_t h;
  char nk[16];
  esp_err_t err;
  nvs_key_of(key, nk, sizeof(nk));
  err = nvs_open(EHC_NVS_NS, NVS_READWRITE, &h);
  if (err != ESP_OK) {
    return 0;
  }
  err = nvs_set_blob(h, nk, buf, len);
  if (err == ESP_OK) {
    err = nvs_commit(h);
  }
  nvs_close(h);
  return err == ESP_OK ? 1 : 0;
}

#if EHC_PORT_ESP32_STORAGE_WORKER
static void nvs_worker(void *arg)
{
  ehc_nvs_job_t *job;
  (void)arg;
  for (;;) {
    if (xQueueReceive(s_nvs_q, &job, portMAX_DELAY) != pdTRUE || !job) {
      continue;
    }
    if (job->is_write) {
      job->result = nvs_write_impl(job->key, job->buf, job->len);
    } else {
      job->result = nvs_read_impl(job->key, job->buf, job->cap, &job->len);
    }
    xSemaphoreGive(job->done);
  }
}

static int nvs_do(int is_write, const char *key, uint8_t *buf, size_t cap_or_len, size_t *len)
{
  ehc_nvs_job_t job;
  ehc_nvs_job_t *pj = &job;
  memset(&job, 0, sizeof(job));
  job.is_write = is_write;
  strncpy(job.key, key ? key : "", sizeof(job.key) - 1);
  job.buf = buf;
  job.cap = cap_or_len;
  job.len = is_write ? cap_or_len : 0;
  job.done = xSemaphoreCreateBinary();
  if (!job.done) {
    return 0;
  }
  if (xQueueSend(s_nvs_q, &pj, pdMS_TO_TICKS(2000)) != pdTRUE) {
    vSemaphoreDelete(job.done);
    return 0;
  }
  if (xSemaphoreTake(job.done, pdMS_TO_TICKS(5000)) != pdTRUE) {
    vSemaphoreDelete(job.done);
    return 0;
  }
  vSemaphoreDelete(job.done);
  if (len) {
    *len = job.len;
  }
  return job.result;
}
#endif

static int esp_store_read(void *ctx, const char *key, uint8_t *buf, size_t cap, size_t *len)
{
  (void)ctx;
#if EHC_PORT_ESP32_STORAGE_WORKER
  return nvs_do(0, key, buf, cap, len);
#else
  return nvs_read_impl(key, buf, cap, len);
#endif
}

static int esp_store_write(void *ctx, const char *key, const uint8_t *buf, size_t len)
{
  (void)ctx;
#if EHC_PORT_ESP32_STORAGE_WORKER
  return nvs_do(1, key, (uint8_t *)buf, len, NULL);
#else
  return nvs_write_impl(key, buf, len);
#endif
}

static uint64_t esp_now(void *ctx)
{
  struct timeval tv;
  (void)ctx;
  if (gettimeofday(&tv, NULL) == 0 && tv.tv_sec > 0) {
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
  }
  return (uint64_t)(esp_timer_get_time() / 1000);
}

static void esp_log_cb(void *ctx, ehc_log_level_t lvl, const char *msg)
{
  (void)ctx;
  if (msg && strncmp(msg, "pending_cmd=", 12) == 0) {
    strncpy(s_last_cmd, msg + 12, sizeof(s_last_cmd) - 1);
    s_last_cmd[sizeof(s_last_cmd) - 1] = '\0';
  }
  if (lvl == EHC_LOG_ERROR) {
    ESP_LOGE(TAG, "%s", msg ? msg : "");
  } else if (lvl == EHC_LOG_WARN) {
    ESP_LOGW(TAG, "%s", msg ? msg : "");
  } else if (lvl == EHC_LOG_DEBUG) {
    ESP_LOGD(TAG, "%s", msg ? msg : "");
  } else {
    ESP_LOGI(TAG, "%s", msg ? msg : "");
  }
}

static void esp_lock(void *ctx)
{
  (void)ctx;
  if (s_mu) {
    xSemaphoreTake(s_mu, portMAX_DELAY);
  }
}

static void esp_unlock(void *ctx)
{
  (void)ctx;
  if (s_mu) {
    xSemaphoreGive(s_mu);
  }
}

int ehc_port_esp32_init(ehc_port_t *out)
{
  if (!out) {
    return 0;
  }
  (void)nvs_flash_init();
  setenv("TZ", "UTC0", 1);
  tzset();
  if (!s_mu) {
    s_mu = xSemaphoreCreateMutex();
  }
#if EHC_PORT_ESP32_STORAGE_WORKER
  if (!s_nvs_q) {
    s_nvs_q = xQueueCreate(4, sizeof(ehc_nvs_job_t *));
  }
  if (!s_nvs_task) {
    (void)xTaskCreate(nvs_worker, "ehc_nvs", 3072, NULL, 5, &s_nvs_task);
  }
#endif
  memset(out, 0, sizeof(*out));
  out->http_post = esp_http;
  out->storage_read = esp_store_read;
  out->storage_write = esp_store_write;
  out->now_ms = esp_now;
  out->log = esp_log_cb;
  out->lock = esp_lock;
  out->unlock = esp_unlock;
  return 1;
}

void ehc_port_esp32_deinit(void)
{
}
