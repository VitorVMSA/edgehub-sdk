#include "ehc_port_linux.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

typedef struct {
  char dir[256];
  uint32_t timeout_ms;
  pthread_mutex_t mu;
  pthread_t thread;
  uint8_t thread_running;
  uint8_t stop;
  uint32_t period_ms;
} ehc_linux_state_t;

static ehc_linux_state_t s_lx;

static void ensure_dir(const char *path)
{
  struct stat st;
  if (stat(path, &st) == 0) {
    return;
  }
  (void)mkdir(path, 0755);
}

static int parse_url(
  const char *url, int *https, char *host, size_t host_cap,
  char *port, size_t port_cap, char *path, size_t path_cap)
{
  const char *p = url;
  const char *slash;
  const char *colon;
  char hostport[192];
  size_t n;
  if (!url || !https || !host || !port || !path) {
    return 0;
  }
  *https = 1;
  strncpy(port, "443", port_cap - 1);
  port[port_cap - 1] = '\0';
  if (strncmp(p, "https://", 8) == 0) {
    p += 8;
  } else if (strncmp(p, "http://", 7) == 0) {
    *https = 0;
    strncpy(port, "80", port_cap - 1);
    p += 7;
  } else {
    return 0;
  }
  slash = strchr(p, '/');
  n = slash ? (size_t)(slash - p) : strlen(p);
  if (n >= sizeof(hostport)) {
    return 0;
  }
  memcpy(hostport, p, n);
  hostport[n] = '\0';
  strncpy(path, slash ? slash : "/", path_cap - 1);
  path[path_cap - 1] = '\0';
  colon = strchr(hostport, ':');
  if (colon) {
    n = (size_t)(colon - hostport);
    if (n >= host_cap) {
      return 0;
    }
    memcpy(host, hostport, n);
    host[n] = '\0';
    strncpy(port, colon + 1, port_cap - 1);
    port[port_cap - 1] = '\0';
  } else {
    strncpy(host, hostport, host_cap - 1);
    host[host_cap - 1] = '\0';
  }
  return host[0] != '\0';
}

static void apply_timeout(BIO *bio, uint32_t timeout_ms)
{
  int fd = -1;
  struct timeval tv;
  if (!bio || timeout_ms == 0) {
    return;
  }
  if (BIO_get_fd(bio, &fd) <= 0 || fd < 0) {
    return;
  }
  tv.tv_sec = (time_t)(timeout_ms / 1000u);
  tv.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);
  (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static uint32_t parse_retry_after(const char *hdr)
{
  const char *p;
  if (!hdr) {
    return 0;
  }
  p = strstr(hdr, "Retry-After:");
  if (!p) {
    p = strstr(hdr, "retry-after:");
  }
  if (!p) {
    return 0;
  }
  p += 12;
  while (*p == ' ') {
    p++;
  }
  return (uint32_t)strtoul(p, NULL, 10);
}

static int linux_http(
  void *ctx, const char *url, const char *headers,
  const uint8_t *body, size_t body_len,
  uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
  uint32_t *retry_after_sec)
{
  int https = 1;
  char host[128];
  char port[16];
  char path[256];
  char hostport[160];
  char request[1024];
  char raw[16384];
  size_t raw_len = 0;
  BIO *bio = NULL;
  SSL_CTX *ssl_ctx = NULL;
  int ok = 0;
  int status = 0;
  char *hdr_end;
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
  resp[0] = '\0';
  if (!parse_url(url, &https, host, sizeof(host), port, sizeof(port), path, sizeof(path))) {
    return 0;
  }

  {
    int n = snprintf(request, sizeof(request), "POST %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", path, host);
    if (n < 0 || (size_t)n >= sizeof(request)) {
      return 0;
    }
    if (headers && headers[0]) {
      if (strlen(request) + strlen(headers) + 80 >= sizeof(request)) {
        return 0;
      }
      strcat(request, headers);
      if (request[strlen(request) - 1] != '\n') {
        strcat(request, "\r\n");
      }
    }
    if (!strstr(request, "Content-Type:") && !strstr(request, "content-type:")) {
      strcat(request, "Content-Type: application/json\r\n");
    }
    n = (int)strlen(request);
    snprintf(request + n, sizeof(request) - (size_t)n, "Content-Length: %zu\r\n\r\n", body_len);
  }

  snprintf(hostport, sizeof(hostport), "%s:%s", host, port);
  if (https) {
    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
      return 0;
    }
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_default_verify_paths(ssl_ctx);
    bio = BIO_new_ssl_connect(ssl_ctx);
    if (!bio) {
      SSL_CTX_free(ssl_ctx);
      return 0;
    }
    {
      SSL *ssl = NULL;
      BIO_get_ssl(bio, &ssl);
      if (ssl) {
        SSL_set_tlsext_host_name(ssl, host);
      }
    }
    BIO_set_conn_hostname(bio, hostport);
    if (BIO_do_connect(bio) <= 0 || BIO_do_handshake(bio) <= 0) {
      BIO_free_all(bio);
      SSL_CTX_free(ssl_ctx);
      return 0;
    }
  } else {
    bio = BIO_new_connect(hostport);
    if (!bio || BIO_do_connect(bio) <= 0) {
      if (bio) {
        BIO_free_all(bio);
      }
      return 0;
    }
  }
  apply_timeout(bio, s_lx.timeout_ms);

  if (BIO_write(bio, request, (int)strlen(request)) <= 0) {
    goto done;
  }
  if (body && body_len > 0 && BIO_write(bio, body, (int)body_len) <= 0) {
    goto done;
  }

  for (;;) {
    int n = BIO_read(bio, raw + raw_len, (int)(sizeof(raw) - 1 - raw_len));
    if (n > 0) {
      raw_len += (size_t)n;
      raw[raw_len] = '\0';
      if (raw_len >= sizeof(raw) - 1) {
        break;
      }
      continue;
    }
    if (BIO_should_retry(bio)) {
      continue;
    }
    break;
  }
  if (raw_len == 0) {
    goto done;
  }
  hdr_end = strstr(raw, "\r\n\r\n");
  if (!hdr_end) {
    goto done;
  }
  if (sscanf(raw, "HTTP/%*s %d", &status) != 1) {
    goto done;
  }
  *http_status = status;
  {
    size_t hlen = (size_t)(hdr_end - raw);
    char hdr_copy[2048];
    const char *bodyp = hdr_end + 4;
    size_t blen = strlen(bodyp);
    if (hlen >= sizeof(hdr_copy)) {
      hlen = sizeof(hdr_copy) - 1;
    }
    memcpy(hdr_copy, raw, hlen);
    hdr_copy[hlen] = '\0';
    if (retry_after_sec) {
      *retry_after_sec = parse_retry_after(hdr_copy);
    }
    if (blen >= resp_cap) {
      blen = resp_cap - 1;
    }
    memcpy(resp, bodyp, blen);
    resp[blen] = '\0';
    if (resp_len) {
      *resp_len = blen;
    }
  }
  ok = 1;

done:
  if (bio) {
    BIO_free_all(bio);
  }
  if (ssl_ctx) {
    SSL_CTX_free(ssl_ctx);
  }
  return ok;
}

static void key_to_path(const char *key, char *out, size_t cap)
{
  size_t i;
  snprintf(out, cap, "%s/", s_lx.dir);
  i = strlen(out);
  while (key && *key && i + 1 < cap) {
    char c = *key++;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') {
      out[i++] = c;
    } else {
      out[i++] = '_';
    }
  }
  out[i] = '\0';
}

static int linux_store_read(void *ctx, const char *key, uint8_t *buf, size_t cap, size_t *len)
{
  char path[320];
  FILE *f;
  size_t n;
  (void)ctx;
  key_to_path(key, path, sizeof(path));
  f = fopen(path, "rb");
  if (!f) {
    return 0;
  }
  n = fread(buf, 1, cap, f);
  fclose(f);
  if (len) {
    *len = n;
  }
  return n > 0 ? 1 : 0;
}

static int linux_store_write(void *ctx, const char *key, const uint8_t *buf, size_t len)
{
  char path[320];
  FILE *f;
  (void)ctx;
  ensure_dir(s_lx.dir);
  key_to_path(key, path, sizeof(path));
  f = fopen(path, "wb");
  if (!f) {
    return 0;
  }
  if (len > 0 && fwrite(buf, 1, len, f) != len) {
    fclose(f);
    return 0;
  }
  fclose(f);
  return 1;
}

static uint64_t linux_now(void *ctx)
{
  struct timespec ts;
  (void)ctx;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}

static void linux_log(void *ctx, ehc_log_level_t lvl, const char *msg)
{
  const char *p = "I";
  (void)ctx;
  if (lvl == EHC_LOG_ERROR) {
    p = "E";
  } else if (lvl == EHC_LOG_WARN) {
    p = "W";
  } else if (lvl == EHC_LOG_DEBUG) {
    p = "D";
  }
  fprintf(stderr, "[ehc:%s] %s\n", p, msg ? msg : "");
}

static void linux_lock(void *ctx)
{
  (void)ctx;
  pthread_mutex_lock(&s_lx.mu);
}

static void linux_unlock(void *ctx)
{
  (void)ctx;
  pthread_mutex_unlock(&s_lx.mu);
}

static void *tick_thread(void *arg)
{
  (void)arg;
  while (!s_lx.stop) {
    (void)ehc_tick();
    {
      uint32_t ms = s_lx.period_ms ? s_lx.period_ms : 1000;
      struct timespec ts;
      ts.tv_sec = (time_t)(ms / 1000u);
      ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
      nanosleep(&ts, NULL);
    }
  }
  return NULL;
}

int ehc_port_linux_init(ehc_port_t *out, const ehc_linux_cfg_t *cfg)
{
  if (!out) {
    return 0;
  }
  memset(&s_lx, 0, sizeof(s_lx));
  pthread_mutex_init(&s_lx.mu, NULL);
  s_lx.timeout_ms = cfg && cfg->timeout_ms ? cfg->timeout_ms : 12000;
  strncpy(
    s_lx.dir,
    (cfg && cfg->storage_dir && cfg->storage_dir[0]) ? cfg->storage_dir : "/var/lib/edgehub-client",
    sizeof(s_lx.dir) - 1);
  SSL_library_init();
  SSL_load_error_strings();
  memset(out, 0, sizeof(*out));
  out->http_post = linux_http;
  out->storage_read = linux_store_read;
  out->storage_write = linux_store_write;
  out->now_ms = linux_now;
  out->log = linux_log;
  out->lock = linux_lock;
  out->unlock = linux_unlock;
  return 1;
}

void ehc_port_linux_deinit(void)
{
  ehc_thread_stop();
  pthread_mutex_destroy(&s_lx.mu);
}

int ehc_thread_start(uint32_t period_ms)
{
  if (s_lx.thread_running) {
    return 1;
  }
  s_lx.stop = 0;
  s_lx.period_ms = period_ms ? period_ms : 1000;
  if (pthread_create(&s_lx.thread, NULL, tick_thread, NULL) != 0) {
    return 0;
  }
  s_lx.thread_running = 1;
  return 1;
}

void ehc_thread_stop(void)
{
  if (!s_lx.thread_running) {
    return;
  }
  s_lx.stop = 1;
  pthread_join(s_lx.thread, NULL);
  s_lx.thread_running = 0;
}
