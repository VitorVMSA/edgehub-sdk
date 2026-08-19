#include "ehc_internal.h"

int ehc_checkin_validate_table(void)
{
  size_t i;
  for (i = 0; i < g_ehc.user.checkin_key_count; i++) {
    if (!g_ehc.user.checkin_keys[i].name || !g_ehc.user.checkin_keys[i].name[0]) {
      ehc_logf(EHC_LOG_ERROR, "check-in key %u has empty name", (unsigned)i);
      return -1;
    }
  }
  return 0;
}

int ehc_asset_version(const char *asset_key, char *out, size_t cap)
{
  char key[96];
  snprintf(key, sizeof(key), "%s%s", EHC_ASSET_PREFIX, asset_key ? asset_key : "");
  return ehc_storage_get(key, out, cap);
}

int ehc_checkin_required_ready(void)
{
  size_t i;
  char buf[128];
  for (i = 0; i < g_ehc.user.checkin_key_count; i++) {
    const ehc_checkin_key_t *k = &g_ehc.user.checkin_keys[i];
    if ((k->flags & EHC_KEY_REQUIRED) == 0) {
      continue;
    }
    buf[0] = '\0';
    if (k->kind == EHC_KEY_KIND_CONST) {
      if (!k->const_value || !k->const_value[0]) {
        return 0;
      }
    } else if (k->kind == EHC_KEY_KIND_FN) {
      if (!k->fn || !k->fn(buf, sizeof(buf)) || !buf[0]) {
        return 0;
      }
    } else if (k->kind == EHC_KEY_KIND_ASSET) {
      if (!ehc_asset_version(k->name, buf, sizeof(buf)) || !buf[0]) {
        return 0;
      }
    }
  }
  return 1;
}

int ehc_checkin_keys_collect(eh_values_writer_t *writer)
{
  size_t i;
  char buf[128];
  for (i = 0; i < g_ehc.user.checkin_key_count; i++) {
    const ehc_checkin_key_t *k = &g_ehc.user.checkin_keys[i];
    buf[0] = '\0';
    if (k->kind == EHC_KEY_KIND_CONST) {
      if (eh_values_add_str(writer, k->name, k->const_value ? k->const_value : "") != EH_OK) {
        return 0;
      }
    } else if (k->kind == EHC_KEY_KIND_FN) {
      if (k->fn && k->fn(buf, sizeof(buf)) && buf[0]) {
        if (eh_values_add_str(writer, k->name, buf) != EH_OK) {
          return 0;
        }
      }
    } else if (k->kind == EHC_KEY_KIND_ASSET) {
      if (ehc_asset_version(k->name, buf, sizeof(buf)) && buf[0]) {
        if (eh_values_add_str(writer, k->name, buf) != EH_OK) {
          return 0;
        }
      }
    }
  }
  return 1;
}

void ehc_on_configuration(const char *asset_key, const char *version, const char *source, const char *payload)
{
  int ok = 1;
  char store_key[96];
  if (!asset_key) {
    return;
  }
  if (g_ehc.user.on_config_apply) {
    ok = g_ehc.user.on_config_apply(asset_key, version, source, payload, g_ehc.user.user_ctx);
  }
  if (!ok) {
    ehc_logf(EHC_LOG_WARN, "config apply failed for %s; version not stored", asset_key);
    return;
  }
  if (!version || !version[0]) {
    return;
  }
  snprintf(store_key, sizeof(store_key), "%s%s", EHC_ASSET_PREFIX, asset_key);
  if (!ehc_storage_put(store_key, version)) {
    ehc_logf(EHC_LOG_WARN, "failed to persist asset version %s", asset_key);
  }
}
