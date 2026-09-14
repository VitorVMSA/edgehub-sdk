#ifndef EH_JSON_H
#define EH_JSON_H

#include "../../include/edgehub_device.h"

int eh_json_escape_append(char *dst, size_t cap, size_t *len, const char *src);
int eh_json_append(char *dst, size_t cap, size_t *len, const char *src);

/** Número JSON no formato de `JSON.stringify` (ECMA-262). NaN/Inf → `null`. */
int eh_json_print_number(double v, char *out, size_t cap);

int eh_find_json_string_value(const char *json, const char *key, char *out, size_t out_cap);
int eh_find_json_raw_value(const char *json, const char *key, char *out, size_t out_cap);
int eh_json_has_token(const char *json, const char *token);

const char *eh_json_find_pending_commands_array(const char *json);
const char *eh_json_next_object(const char *cursor, const char **obj_end);

/**
 * Itera entradas `"key": value` de um objecto JSON.
 * @param cursor In/out: após `{` do objecto; avança para depois de cada entrada.
 * @return 1 se leu entrada, 0 se fim do objecto, -1 em erro de parse.
 */
int eh_json_next_map_entry(
  const char **cursor,
  char *key_out,
  size_t key_cap,
  char *value_out,
  size_t value_cap
);

#endif /* EH_JSON_H */
