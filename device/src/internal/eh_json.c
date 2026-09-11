#include "eh_json.h"

#include <string.h>

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
