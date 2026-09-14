#include "eh_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int copy_lit(char *out, size_t cap, const char *s)
{
  size_t n = strlen(s);
  if (n + 1 > cap) return 0;
  memcpy(out, s, n + 1);
  return 1;
}

static int emit_ch(char *out, size_t cap, size_t *o, char c)
{
  if (*o + 1 >= cap) return 0;
  out[(*o)++] = c;
  return 1;
}

static int emit_str(char *out, size_t cap, size_t *o, const char *s)
{
  while (*s) {
    if (!emit_ch(out, cap, o, *s++)) return 0;
  }
  return 1;
}

static int es_format_abs(const char *raw, char *out, size_t cap)
{
  char digits[40];
  char expbuf[16];
  const char *p;
  const char *epos;
  size_t o = 0;
  int E = 0;
  int point_index;
  int leading_zeros;
  int k = 0;
  int n;
  int i;
  int exp;

  if (!raw || !raw[0] || !out || cap < 2) return 0;
  if (raw[0] == 'i' || raw[0] == 'I' || raw[0] == 'n' || raw[0] == 'N') {
    return 0;
  }

  epos = strchr(raw, 'e');
  if (!epos) epos = strchr(raw, 'E');
  if (epos) {
    E = atoi(epos + 1);
  }

  point_index = -1;
  for (p = raw; epos ? p < epos : *p; p++) {
    if (*p == '.' || *p == ',') {
      point_index = k;
      continue;
    }
    if (*p >= '0' && *p <= '9') {
      if (k < (int)sizeof(digits) - 1) digits[k++] = *p;
    }
  }
  if (k == 0) return copy_lit(out, cap, "0");
  if (point_index < 0) point_index = k;
  digits[k] = '\0';

  leading_zeros = 0;
  i = 0;
  while (i < k - 1 && digits[i] == '0') {
    leading_zeros++;
    i++;
  }
  if (leading_zeros) {
    memmove(digits, digits + leading_zeros, (size_t)(k - leading_zeros) + 1);
    k -= leading_zeros;
  }
  while (k > 1 && digits[k - 1] == '0') {
    k--;
  }
  digits[k] = '\0';
  n = point_index + E - leading_zeros;

  if (k <= n && n <= 21) {
    for (i = 0; i < k; i++) {
      if (!emit_ch(out, cap, &o, digits[i])) return 0;
    }
    for (i = 0; i < n - k; i++) {
      if (!emit_ch(out, cap, &o, '0')) return 0;
    }
  } else if (n > 0 && n <= 21) {
    for (i = 0; i < n; i++) {
      if (!emit_ch(out, cap, &o, digits[i])) return 0;
    }
    if (!emit_ch(out, cap, &o, '.')) return 0;
    for (i = n; i < k; i++) {
      if (!emit_ch(out, cap, &o, digits[i])) return 0;
    }
  } else if (n > -6 && n <= 0) {
    if (!emit_ch(out, cap, &o, '0')) return 0;
    if (!emit_ch(out, cap, &o, '.')) return 0;
    for (i = 0; i < -n; i++) {
      if (!emit_ch(out, cap, &o, '0')) return 0;
    }
    for (i = 0; i < k; i++) {
      if (!emit_ch(out, cap, &o, digits[i])) return 0;
    }
  } else {
    if (!emit_ch(out, cap, &o, digits[0])) return 0;
    if (k > 1) {
      if (!emit_ch(out, cap, &o, '.')) return 0;
      for (i = 1; i < k; i++) {
        if (!emit_ch(out, cap, &o, digits[i])) return 0;
      }
    }
    if (!emit_ch(out, cap, &o, 'e')) return 0;
    exp = n - 1;
    if (exp >= 0 && !emit_ch(out, cap, &o, '+')) return 0;
    if (snprintf(expbuf, sizeof(expbuf), "%d", exp) <= 0) return 0;
    if (!emit_str(out, cap, &o, expbuf)) return 0;
  }
  if (o >= cap) return 0;
  out[o] = '\0';
  return 1;
}

/**
 * Número JSON igual a JSON.stringify (ECMA-262 NumberToString).
 * O ingest HMAC reconstrói o canónico com JSON.stringify; %.15g diverge
 * em |x|<1e-4 (1e-05 vs 0.00001) e no expoente com zeros (e-007 vs e-7).
 */
int eh_json_print_number(double v, char *out, size_t cap)
{
  char raw[64];
  char abs_fmt[64];
  char cand[68];
  char *end;
  double parsed;
  int sign;
  int prec;
  int ok = 0;
  double absv;

  if (!out || cap < 2) return 0;
  if (v != v) return copy_lit(out, cap, "null");
  if (v == 0.0) return copy_lit(out, cap, "0");
  if (v * 2.0 == v) return copy_lit(out, cap, "null");

  sign = v < 0.0;
  absv = sign ? -v : v;

  for (prec = 1; prec <= 17; prec++) {
    if (snprintf(raw, sizeof(raw), "%.*g", prec, absv) <= 0) continue;
    if (!es_format_abs(raw, abs_fmt, sizeof(abs_fmt))) continue;
    if (sign) {
      if (sizeof(cand) < strlen(abs_fmt) + 2) continue;
      cand[0] = '-';
      memcpy(cand + 1, abs_fmt, strlen(abs_fmt) + 1);
    } else {
      memcpy(cand, abs_fmt, strlen(abs_fmt) + 1);
    }
    end = NULL;
    parsed = strtod(cand, &end);
    if (!end || *end != '\0') continue;
    if (parsed != v) continue;
    return copy_lit(out, cap, cand);
  }

  if (snprintf(raw, sizeof(raw), "%.17g", absv) <= 0) return 0;
  ok = es_format_abs(raw, abs_fmt, sizeof(abs_fmt));
  if (!ok) return 0;
  if (sign) {
    if (cap < strlen(abs_fmt) + 2) return 0;
    out[0] = '-';
    memcpy(out + 1, abs_fmt, strlen(abs_fmt) + 1);
    return 1;
  }
  return copy_lit(out, cap, abs_fmt);
}

int eh_json_append(char *dst, size_t cap, size_t *len, const char *src) {
  size_t n = strlen(src);
  if (!dst || !len || *len + n + 1 > cap) return 0;
  memcpy(dst + *len, src, n);
  *len += n;
  dst[*len] = '\0';
  return 1;
}

int eh_json_escape_append(char *dst, size_t cap, size_t *len, const char *src) {
  size_t i = 0;
  if (!dst || !len || !src) return 0;
  while (src[i]) {
    char c = src[i++];
    if (c == '"' || c == '\\') {
      if (*len + 2 + 1 > cap) return 0;
      dst[(*len)++] = '\\';
      dst[(*len)++] = c;
      continue;
    }
    if (*len + 1 + 1 > cap) return 0;
    dst[(*len)++] = c;
  }
  dst[*len] = '\0';
  return 1;
}

int eh_find_json_string_value(const char *json, const char *key, char *out, size_t out_cap) {
  char needle[96];
  const char *p;
  const char *start;
  size_t n = 0;
  if (!json || !key || !out || out_cap == 0) return 0;
  out[0] = '\0';
  if (strlen(key) + 3 >= sizeof(needle)) return 0;
  needle[0] = '"';
  strcpy(&needle[1], key);
  strcat(needle, "\":");
  p = strstr(json, needle);
  if (!p) return 0;
  start = p + strlen(needle);
  while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
  if (*start != '"') return 0;
  start++;
  while (start[n] && start[n] != '"') {
    if (start[n] == '\\' && start[n + 1]) n += 2;
    else n++;
  }
  if (n == 0 || n + 1 > out_cap) return 0;
  memcpy(out, start, n);
  out[n] = '\0';
  return 1;
}

int eh_find_json_raw_value(const char *json, const char *key, char *out, size_t out_cap) {
  char needle[96];
  const char *p;
  const char *start;
  int depth = 0;
  size_t n = 0;
  char opener;
  char closer;
  if (!json || !key || !out || out_cap == 0) return 0;
  out[0] = '\0';
  if (strlen(key) + 3 >= sizeof(needle)) return 0;
  needle[0] = '"';
  strcpy(&needle[1], key);
  strcat(needle, "\":");
  p = strstr(json, needle);
  if (!p) return 0;
  start = p + strlen(needle);
  while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
  if (*start != '{' && *start != '[') {
    return eh_find_json_string_value(json, key, out, out_cap);
  }
  opener = *start;
  closer = (opener == '{') ? '}' : ']';
  depth = 1;
  n = 1;
  while (start[n] && depth > 0) {
    if (start[n] == opener) depth++;
    else if (start[n] == closer) depth--;
    n++;
  }
  if (depth != 0 || n + 1 > out_cap) return 0;
  memcpy(out, start, n);
  out[n] = '\0';
  return 1;
}

int eh_json_has_token(const char *json, const char *token) {
  if (!json || !token) return 0;
  return strstr(json, token) != 0;
}

const char *eh_json_find_pending_commands_array(const char *json) {
  const char *p;
  if (!json) return 0;
  p = strstr(json, "\"pendingCommands\"");
  if (!p) return 0;
  p = strchr(p, '[');
  return p;
}

const char *eh_json_next_object(const char *cursor, const char **obj_end) {
  const char *start;
  const char *p;
  int depth;
  if (!cursor) return 0;
  start = strchr(cursor, '{');
  if (!start) return 0;
  depth = 1;
  p = start + 1;
  while (*p && depth > 0) {
    if (*p == '{') depth++;
    else if (*p == '}') depth--;
    p++;
  }
  if (depth != 0) return 0;
  if (obj_end) *obj_end = p;
  return start;
}

static void eh_json_skip_ws(const char **p) {
  if (!p || !*p) return;
  while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

static int eh_json_read_quoted_string(const char **p, char *out, size_t cap) {
  size_t n = 0;
  if (!p || !*p || !out || cap == 0) return 0;
  if (**p != '"') return 0;
  (*p)++;
  while (**p && **p != '"') {
    if (**p == '\\' && (*p)[1]) {
      if (n + 1 >= cap) return 0;
      out[n++] = (*p)[1];
      (*p) += 2;
      continue;
    }
    if (n + 1 >= cap) return 0;
    out[n++] = **p;
    (*p)++;
  }
  if (**p != '"') return 0;
  (*p)++;
  out[n] = '\0';
  return 1;
}

static int eh_json_copy_value_at(const char **p, char *out, size_t cap) {
  const char *start;
  int depth;
  char opener;
  char closer;
  size_t n;
  if (!p || !*p || !out || cap == 0) return 0;
  eh_json_skip_ws(p);
  start = *p;
  if (**p == '"') {
    if (!eh_json_read_quoted_string(p, out, cap)) return 0;
    return 1;
  }
  if (**p == '{' || **p == '[') {
    opener = **p;
    closer = (opener == '{') ? '}' : ']';
    depth = 1;
    (*p)++;
    while (**p && depth > 0) {
      if (**p == '"') {
        (*p)++;
        while (**p && **p != '"') {
          if (**p == '\\' && (*p)[1]) (*p) += 2;
          else (*p)++;
        }
        if (**p == '"') (*p)++;
        continue;
      }
      if (**p == opener) depth++;
      else if (**p == closer) depth--;
      (*p)++;
    }
    n = (size_t)(*p - start);
    if (n + 1 > cap) return 0;
    memcpy(out, start, n);
    out[n] = '\0';
    return 1;
  }
  while (**p && **p != ',' && **p != '}' && **p != ']') (*p)++;
  n = (size_t)(*p - start);
  if (n + 1 > cap) return 0;
  memcpy(out, start, n);
  out[n] = '\0';
  return 1;
}

int eh_json_next_map_entry(
  const char **cursor,
  char *key_out,
  size_t key_cap,
  char *value_out,
  size_t value_cap
) {
  if (!cursor || !*cursor || !key_out || key_cap == 0 || !value_out || value_cap == 0) return -1;
  eh_json_skip_ws(cursor);
  if (**cursor == '}') return 0;
  if (**cursor == ',') {
    (*cursor)++;
    eh_json_skip_ws(cursor);
  }
  if (**cursor == '}') return 0;
  if (!eh_json_read_quoted_string(cursor, key_out, key_cap)) return -1;
  eh_json_skip_ws(cursor);
  if (**cursor != ':') return -1;
  (*cursor)++;
  eh_json_skip_ws(cursor);
  if (!eh_json_copy_value_at(cursor, value_out, value_cap)) return -1;
  eh_json_skip_ws(cursor);
  return 1;
}
