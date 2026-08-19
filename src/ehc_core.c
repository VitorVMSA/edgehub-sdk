#include "ehc_internal.h"

ehc_ctx_t g_ehc;

void ehc_lock(void)
{
  if (g_ehc.port.lock) {
    g_ehc.port.lock(g_ehc.port.ctx);
  }
}

void ehc_unlock(void)
{
  if (g_ehc.port.unlock) {
    g_ehc.port.unlock(g_ehc.port.ctx);
  }
}

uint64_t ehc_now(void)
{
  if (g_ehc.port.now_ms) {
    return g_ehc.port.now_ms(g_ehc.port.ctx);
  }
  return 0;
}

void ehc_logf(ehc_log_level_t lvl, const char *fmt, ...)
{
  char buf[192];
  va_list ap;
  if (!g_ehc.port.log) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  g_ehc.port.log(g_ehc.port.ctx, lvl, buf);
}

int ehc_storage_get(const char *key, char *out, size_t cap)
{
  uint8_t buf[256];
  size_t len = 0;
  if (!g_ehc.port.storage_read || !key || !out || cap == 0) {
    return 0;
  }
  if (!g_ehc.port.storage_read(g_ehc.port.ctx, key, buf, sizeof(buf) - 1, &len) || len == 0) {
    out[0] = '\0';
    return 0;
  }
  if (len >= cap) {
    len = cap - 1;
  }
  memcpy(out, buf, len);
  out[len] = '\0';
  return 1;
}

int ehc_storage_put(const char *key, const char *value)
{
  if (!g_ehc.port.storage_write || !key) {
    return 0;
  }
  value = value ? value : "";
  return g_ehc.port.storage_write(g_ehc.port.ctx, key, (const uint8_t *)value, strlen(value) + 1) ? 1 : 0;
}

int ehc_resolve_ident(const ehc_identity_source_t *src, char *out, size_t cap)
{
  if (!out || cap == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!src) {
    return 0;
  }
  if (src->kind == EHC_IDENT_LITERAL) {
    if (!src->value || !src->value[0]) {
      return 0;
    }
    strncpy(out, src->value, cap - 1);
    out[cap - 1] = '\0';
    return 1;
  }
  if (src->kind == EHC_IDENT_STORAGE) {
    return ehc_storage_get(src->value, out, cap);
  }
  if (src->kind == EHC_IDENT_FN && src->fn) {
    return src->fn(out, cap) && out[0] ? 1 : 0;
  }
  return 0;
}

static int ident_nonempty(const ehc_identity_source_t *src)
{
  char buf[EHC_IDENT_MAX];
  return ehc_resolve_ident(src, buf, sizeof(buf));
}

void ehc_refresh_readiness(void)
{
  char base[EHC_IDENT_MAX];
  char uid[EHC_IDENT_MAX];
  char gk[EHC_IDENT_MAX];
  char pc[EHC_IDENT_MAX];
  uint8_t has_secret;

  g_ehc.not_ready[0] = '\0';
  if (!ident_nonempty(&g_ehc.user.base_url)) {
    g_ehc.readiness = EHC_UNCONFIGURED;
    snprintf(g_ehc.not_ready, sizeof(g_ehc.not_ready), "missing base_url");
    return;
  }
  if (!ident_nonempty(&g_ehc.user.device_uid)) {
    g_ehc.readiness = EHC_UNCONFIGURED;
    snprintf(g_ehc.not_ready, sizeof(g_ehc.not_ready), "missing device_uid");
    return;
  }
  (void)base;
  (void)uid;
  if (!ehc_checkin_required_ready()) {
    g_ehc.readiness = EHC_UNCONFIGURED;
    snprintf(g_ehc.not_ready, sizeof(g_ehc.not_ready), "missing required check-in key");
    return;
  }
  if (!ehc_resolve_ident(&g_ehc.user.group_key, gk, sizeof(gk)) ||
      !ehc_resolve_ident(&g_ehc.user.product_code, pc, sizeof(pc))) {
    g_ehc.readiness = EHC_MINIMAL;
    snprintf(g_ehc.not_ready, sizeof(g_ehc.not_ready), "waiting group_key+product_code");
    return;
  }
  has_secret = 0;
  if (g_ehc.eh_inited) {
    has_secret = g_ehc.eh.has_secret_current ? 1 : 0;
  }
  if (g_ehc.eh_inited && g_ehc.eh.needs_credential_reprovision) {
    g_ehc.readiness = EHC_FULL;
    snprintf(g_ehc.not_ready, sizeof(g_ehc.not_ready), "waiting hmac secret reprovision");
    return;
  }
  if (has_secret && g_ehc.eh.provisioning == EH_PROV_ACTIVE) {
    g_ehc.readiness = EHC_ACTIVE;
    g_ehc.not_ready[0] = '\0';
    return;
  }
  g_ehc.readiness = EHC_FULL;
  if (!has_secret) {
    snprintf(g_ehc.not_ready, sizeof(g_ehc.not_ready), "waiting hmac secret");
  } else {
    snprintf(g_ehc.not_ready, sizeof(g_ehc.not_ready), "waiting provisioning active");
  }
}

void ehc_load_secret(void)
{
  char cur[EH_MAX_SECRET];
  char prev[EH_MAX_SECRET];
  int has_cur = ehc_storage_get(EHC_STORE_SECRET, cur, sizeof(cur));
  int has_prev = ehc_storage_get(EHC_STORE_SECRET_PREV, prev, sizeof(prev));
  if (has_cur && g_ehc.eh_inited) {
    (void)eh_client_set_secret(&g_ehc.eh, cur, has_prev ? prev : NULL);
    strncpy(g_ehc.last_secret, cur, sizeof(g_ehc.last_secret) - 1);
  }
}

void ehc_persist_secret_if_changed(void)
{
  if (!g_ehc.eh_inited || !g_ehc.eh.has_secret_current) {
    return;
  }
  if (strcmp(g_ehc.last_secret, g_ehc.eh.secret_current) == 0) {
    return;
  }
  if (g_ehc.last_secret[0]) {
    (void)ehc_storage_put(EHC_STORE_SECRET_PREV, g_ehc.last_secret);
  }
  (void)ehc_storage_put(EHC_STORE_SECRET, g_ehc.eh.secret_current);
  strncpy(g_ehc.last_secret, g_ehc.eh.secret_current, sizeof(g_ehc.last_secret) - 1);
}

ehc_status_t ehc_ensure_eh_client(void)
{
  eh_config_t cfg;
  eh_callbacks_t cb;
  char base[EHC_IDENT_MAX];
  char slug[EHC_IDENT_MAX];
  char uid[EHC_IDENT_MAX];
  char gk[EHC_IDENT_MAX];
  char pc[EHC_IDENT_MAX];
  char pkcs8[512];

  if (g_ehc.eh_inited) {
    if (ehc_resolve_ident(&g_ehc.user.group_key, gk, sizeof(gk)) &&
        ehc_resolve_ident(&g_ehc.user.product_code, pc, sizeof(pc))) {
      (void)eh_client_set_group_product(&g_ehc.eh, gk, pc);
    }
    return EHC_OK;
  }

  memset(&cfg, 0, sizeof(cfg));
  memset(&cb, 0, sizeof(cb));
  if (!ehc_resolve_ident(&g_ehc.user.base_url, base, sizeof(base)) ||
      !ehc_resolve_ident(&g_ehc.user.device_uid, uid, sizeof(uid))) {
    return EHC_ENOTREADY;
  }
  strncpy(cfg.base_url, base, sizeof(cfg.base_url) - 1);
  strncpy(cfg.device_uid, uid, sizeof(cfg.device_uid) - 1);
  if (ehc_resolve_ident(&g_ehc.user.tenant_slug, slug, sizeof(slug))) {
    strncpy(cfg.tenant_slug, slug, sizeof(cfg.tenant_slug) - 1);
  }
  if (ehc_resolve_ident(&g_ehc.user.group_key, gk, sizeof(gk)) &&
      ehc_resolve_ident(&g_ehc.user.product_code, pc, sizeof(pc))) {
    strncpy(cfg.group_key, gk, sizeof(cfg.group_key) - 1);
    strncpy(cfg.product_code, pc, sizeof(cfg.product_code) - 1);
  }
  cfg.enable_logs = 1;
  cfg.log_level = EH_LOG_INFO;
  cfg.enable_checkin = 1;
  cfg.enable_telemetry = 1;
  cfg.enable_commands = 1;
  cfg.enable_auto_confirm_commands = 1;
  cfg.checkin_on_keys_change = 1;
  cfg.telemetry_manual_only = g_ehc.user.telemetry_manual_only ? 1 : 0;
  cfg.enable_identity_credential_issue = g_ehc.user.enable_identity_credential_issue ? 1 : 0;
  cfg.checkin_interval_ms = (uint32_t)EHC_CHECKIN_INTERVAL_MS;
  cfg.telemetry_interval_ms = (uint32_t)EHC_TELEMETRY_INTERVAL_MS;
  cfg.telemetry_batch_max = (uint8_t)EHC_TELEMETRY_BATCH_MAX;

  cb.http_post_ex = ehc_bridge_http;
  cb.store_read = g_ehc.port.storage_read ? ehc_bridge_store_read : NULL;
  cb.store_write = g_ehc.port.storage_write ? ehc_bridge_store_write : NULL;
  cb.telemetry_collect = ehc_bridge_telemetry_collect;
  cb.checkin_keys_collect = ehc_bridge_checkin_keys;
  cb.on_command = ehc_bridge_on_command;
  cb.on_command_message = ehc_bridge_on_command_message;
  cb.on_checkin_download = ehc_bridge_on_download;
  cb.on_checkin_configuration = ehc_bridge_on_configuration;
  cb.log_fn = ehc_bridge_log;
  cb.user_ctx = &g_ehc;

  if (eh_client_init(&g_ehc.eh, &cfg, &cb, ehc_now()) != EH_OK) {
    return EHC_EIO;
  }
  g_ehc.eh_inited = 1;
  ehc_load_secret();
  if (ehc_resolve_ident(&g_ehc.user.identity_pkcs8, pkcs8, sizeof(pkcs8))) {
    (void)eh_client_set_identity_private_key_pkcs8_b64(&g_ehc.eh, pkcs8);
  }
  return EHC_OK;
}

static ehc_status_t validate_user(const ehc_user_t *user)
{
  if (!user) {
    return EHC_EINVAL;
  }
  if (user->metric_count > EHC_MAX_METRICS) {
    ehc_logf(EHC_LOG_ERROR, "metric table exceeds EHC_MAX_METRICS (%u)", (unsigned)EHC_MAX_METRICS);
    return EHC_ECONFIG;
  }
  if (user->command_count > EHC_MAX_COMMANDS_TABLE) {
    ehc_logf(EHC_LOG_ERROR, "command table exceeds EHC_MAX_COMMANDS_TABLE");
    return EHC_ECONFIG;
  }
  if (user->checkin_key_count > EHC_MAX_CHECKIN_KEYS) {
    ehc_logf(EHC_LOG_ERROR, "check-in key table exceeds EHC_MAX_CHECKIN_KEYS");
    return EHC_ECONFIG;
  }
  if (ehc_telemetry_validate_table() != 0) {
    return EHC_ECONFIG;
  }
  if (ehc_checkin_validate_table() != 0) {
    return EHC_ECONFIG;
  }
  if (ehc_commands_validate_table() != 0) {
    return EHC_ECONFIG;
  }
  return EHC_OK;
}

ehc_status_t ehc_init(const ehc_port_t *port, const ehc_user_t *user)
{
  ehc_status_t rc;
  if (!port || !port->http_post || !port->now_ms || !user) {
    return EHC_EINVAL;
  }
  memset(&g_ehc, 0, sizeof(g_ehc));
  g_ehc.port = *port;
  g_ehc.user = *user;
  g_ehc.used = 1;
  if (eh_lib_abi_check() != EH_OK) {
    ehc_logf(EHC_LOG_ERROR, "edgehub_device ABI mismatch (%s)", eh_lib_version_string());
    g_ehc.used = 0;
    return EHC_EABI;
  }
  rc = validate_user(user);
  if (rc != EHC_OK) {
    g_ehc.used = 0;
    return rc;
  }
  ehc_refresh_readiness();
  return EHC_OK;
}

void ehc_shutdown(void)
{
  memset(&g_ehc, 0, sizeof(g_ehc));
}

int ehc_is_initialized(void)
{
  return g_ehc.used ? 1 : 0;
}

ehc_readiness_t ehc_get_readiness(void)
{
  if (!g_ehc.used) {
    return EHC_UNCONFIGURED;
  }
  ehc_refresh_readiness();
  return g_ehc.readiness;
}

const char *ehc_readiness_name(ehc_readiness_t r)
{
  switch (r) {
    case EHC_MINIMAL: return "MINIMAL";
    case EHC_FULL: return "FULL";
    case EHC_ACTIVE: return "ACTIVE";
    default: return "UNCONFIGURED";
  }
}

const char *ehc_not_ready_reason(void)
{
  return g_ehc.used ? g_ehc.not_ready : "not initialized";
}

ehc_status_t ehc_tick(void)
{
  uint64_t now;
  ehc_status_t rc;
  if (!g_ehc.used) {
    return EHC_EINVAL;
  }
  ehc_lock();
  ehc_refresh_readiness();
  now = ehc_now();
  if (g_ehc.readiness == EHC_UNCONFIGURED) {
    if (g_ehc.last_not_ready_log_ms == 0 ||
        now - g_ehc.last_not_ready_log_ms >= EHC_NOT_READY_LOG_MS) {
      ehc_logf(EHC_LOG_WARN, "not ready: %s", g_ehc.not_ready);
      g_ehc.last_not_ready_log_ms = now == 0 ? 1 : now;
    }
    ehc_unlock();
    return EHC_ENOTREADY;
  }
  rc = ehc_ensure_eh_client();
  if (rc != EHC_OK) {
    ehc_unlock();
    return rc;
  }
  if (eh_client_tick(&g_ehc.eh, now) != EH_OK) {
    rc = EHC_EIO;
  }
  ehc_persist_secret_if_changed();
  ehc_refresh_readiness();
  if (g_ehc.readiness == EHC_ACTIVE && eh_client_telemetry_queue_count(&g_ehc.eh) > 0) {
    (void)eh_client_flush_telemetry(&g_ehc.eh, now);
  }
  ehc_run_deferred();
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_force_checkin(void)
{
  ehc_status_t rc;
  if (!g_ehc.used) {
    return EHC_EINVAL;
  }
  ehc_lock();
  ehc_refresh_readiness();
  if (g_ehc.readiness == EHC_UNCONFIGURED) {
    ehc_unlock();
    return EHC_ENOTREADY;
  }
  rc = ehc_ensure_eh_client();
  if (rc == EHC_OK) {
    rc = (eh_client_force_checkin(&g_ehc.eh, ehc_now()) == EH_OK) ? EHC_OK : EHC_EIO;
    ehc_persist_secret_if_changed();
  }
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_publish(void)
{
  ehc_status_t rc;
  uint64_t now;
  if (!g_ehc.used) {
    return EHC_EINVAL;
  }
  ehc_lock();
  ehc_refresh_readiness();
  if (g_ehc.readiness == EHC_UNCONFIGURED) {
    ehc_unlock();
    return EHC_ENOTREADY;
  }
  rc = ehc_ensure_eh_client();
  if (rc != EHC_OK) {
    ehc_unlock();
    return rc;
  }
  now = ehc_now();
  if (eh_client_enqueue_reading(&g_ehc.eh, NULL, now) != EH_OK) {
    ehc_unlock();
    return EHC_EIO;
  }
  ehc_refresh_readiness();
  if (g_ehc.readiness == EHC_ACTIVE) {
    rc = (eh_client_flush_telemetry(&g_ehc.eh, now) == EH_OK) ? EHC_OK : EHC_EIO;
  } else {
    rc = EHC_OK;
  }
  ehc_run_deferred();
  ehc_unlock();
  return rc;
}
