#include "ehc_internal.h"
#include <stdlib.h>

static const char *skip_ws(const char *p)
{
  while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
    p++;
  }
  return p;
}

static int unescape_into(const char *src, size_t len, char *out, size_t cap)
{
  size_t i = 0;
  size_t o = 0;
  if (!out || cap == 0) {
    return 0;
  }
  while (i < len && o + 1 < cap) {
    if (src[i] == '\\' && i + 1 < len) {
      i++;
      out[o++] = src[i++];
      continue;
    }
    out[o++] = src[i++];
  }
  out[o] = '\0';
  return 1;
}

static const char *skip_value(const char *p, int depth)
{
  p = skip_ws(p);
  if (!p || !*p) {
    return NULL;
  }
  if (*p == '"') {
    p++;
    while (*p) {
      if (*p == '\\' && p[1]) {
        p += 2;
        continue;
      }
      if (*p == '"') {
        return p + 1;
      }
      p++;
    }
    return NULL;
  }
  if (*p == '{') {
    int d = 1;
    p++;
    if (depth + 1 > EHC_ARGS_MAX_DEPTH) {
      return NULL;
    }
    while (*p && d) {
      if (*p == '"') {
        p = skip_value(p, depth);
        if (!p) {
          return NULL;
        }
        continue;
      }
      if (*p == '{') {
        d++;
      } else if (*p == '}') {
        d--;
      }
      p++;
    }
    return p;
  }
  if (*p == '[') {
    int d = 1;
    p++;
    while (*p && d) {
      if (*p == '"') {
        p = skip_value(p, depth);
        if (!p) {
          return NULL;
        }
        continue;
      }
      if (*p == '[') {
        d++;
      } else if (*p == ']') {
        d--;
      }
      p++;
    }
    return p;
  }
  if (!strncmp(p, "true", 4)) {
    return p + 4;
  }
  if (!strncmp(p, "false", 5)) {
    return p + 5;
  }
  if (!strncmp(p, "null", 4)) {
    return p + 4;
  }
  if (*p == '-' || (*p >= '0' && *p <= '9')) {
    if (*p == '-') {
      p++;
    }
    while (*p >= '0' && *p <= '9') {
      p++;
    }
    if (*p == '.') {
      p++;
      while (*p >= '0' && *p <= '9') {
        p++;
      }
    }
    if (*p == 'e' || *p == 'E') {
      p++;
      if (*p == '+' || *p == '-') {
        p++;
      }
      while (*p >= '0' && *p <= '9') {
        p++;
      }
    }
    return p;
  }
  return NULL;
}

int ehc_json_parse_args(const char *payload, ehc_args_t *out)
{
  const char *p;
  memset(out, 0, sizeof(*out));
  out->payload = payload ? payload : "{}";
  p = skip_ws(out->payload);
  if (!p || *p != '{') {
    return 0;
  }
  p++;
  while (p && *p) {
    const char *key_s;
    const char *key_e;
    const char *val_s;
    const char *val_e;
    ehc_arg_entry_t *e;
    p = skip_ws(p);
    if (*p == '}') {
      return 1;
    }
    if (*p != '"') {
      return 0;
    }
    p++;
    key_s = p;
    while (*p && *p != '"') {
      if (*p == '\\' && p[1]) {
        p += 2;
        continue;
      }
      p++;
    }
    if (*p != '"') {
      return 0;
    }
    key_e = p;
    p++;
    p = skip_ws(p);
    if (*p != ':') {
      return 0;
    }
    p++;
    p = skip_ws(p);
    val_s = p;
    val_e = skip_value(p, 1);
    if (!val_e) {
      return 0;
    }
    if (out->count < EHC_ARGS_MAX_KEYS) {
      e = &out->entries[out->count++];
      e->raw = val_s;
      e->raw_len = (size_t)(val_e - val_s);
      unescape_into(key_s, (size_t)(key_e - key_s), e->decoded, sizeof(e->decoded));
      e->key = e->decoded;
      if (*val_s == '"') {
        e->kind = 0;
      } else if (*val_s == 't' || *val_s == 'f') {
        e->kind = 2;
      } else if (*val_s == '-' || (*val_s >= '0' && *val_s <= '9')) {
        e->kind = 1;
      } else {
        e->kind = 3;
      }
    }
    p = skip_ws(val_e);
    if (*p == ',') {
      p++;
      continue;
    }
    if (*p == '}') {
      return 1;
    }
    return 0;
  }
  return 0;
}

static const ehc_arg_entry_t *find_arg(const ehc_args_t *args, const char *key)
{
  int i;
  if (!args || !key) {
    return NULL;
  }
  for (i = 0; i < args->count; i++) {
    if (args->entries[i].key && strcmp(args->entries[i].key, key) == 0) {
      return &args->entries[i];
    }
  }
  return NULL;
}

int ehc_args_has(const ehc_args_t *args, const char *key)
{
  return find_arg(args, key) != NULL;
}

double ehc_args_num(const ehc_args_t *args, const char *key, double def)
{
  const ehc_arg_entry_t *e = find_arg(args, key);
  char tmp[64];
  if (!e || (e->kind != 1 && e->kind != 0)) {
    return def;
  }
  if (e->raw_len >= sizeof(tmp)) {
    return def;
  }
  memcpy(tmp, e->raw, e->raw_len);
  tmp[e->raw_len] = '\0';
  if (e->kind == 0 && tmp[0] == '"') {
    char inner[64];
    unescape_into(e->raw + 1, e->raw_len >= 2 ? e->raw_len - 2 : 0, inner, sizeof(inner));
    return strtod(inner, NULL);
  }
  return strtod(tmp, NULL);
}

long ehc_args_int(const ehc_args_t *args, const char *key, long def)
{
  return (long)ehc_args_num(args, key, (double)def);
}

int ehc_args_bool(const ehc_args_t *args, const char *key, int def)
{
  const ehc_arg_entry_t *e = find_arg(args, key);
  if (!e) {
    return def;
  }
  if (e->kind == 2) {
    return e->raw[0] == 't';
  }
  if (e->kind == 1) {
    return ehc_args_num(args, key, 0) != 0.0;
  }
  return def;
}

const char *ehc_args_str(const ehc_args_t *args, const char *key, const char *def)
{
  static char decoded[96];
  const ehc_arg_entry_t *e = find_arg(args, key);
  if (!e || e->kind != 0 || e->raw_len < 2) {
    return def;
  }
  unescape_into(e->raw + 1, e->raw_len - 2, decoded, sizeof(decoded));
  return decoded;
}

const char *ehc_args_raw(const ehc_args_t *args, const char *key)
{
  const ehc_arg_entry_t *e = find_arg(args, key);
  return e ? e->raw : NULL;
}
