#include "eh_b64.h"

#include <ctype.h>
#include <string.h>

static int b64_value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  if (c == '-') return 62;
  if (c == '_') return 63;
  return -1;
}

eh_status_t eh_b64_decode(const char *in, uint8_t *out, size_t out_cap, size_t *out_len) {
  size_t len;
  size_t i;
  size_t o = 0;
  int val[4];

  if (!in || !out || !out_len) return EH_ERR_INVALID_ARG;
  len = strlen(in);
  while (len > 0 && in[len - 1] == '=') len--;

  for (i = 0; i + 3 < len; i += 4) {
    val[0] = b64_value(in[i]);
    val[1] = b64_value(in[i + 1]);
    val[2] = b64_value(in[i + 2]);
    val[3] = b64_value(in[i + 3]);
    if (val[0] < 0 || val[1] < 0 || val[2] < 0 || val[3] < 0) return EH_ERR_PARSE;
    if (o + 3 > out_cap) return EH_ERR_BUFFER_TOO_SMALL;
    out[o++] = (uint8_t)((val[0] << 2) | (val[1] >> 4));
    out[o++] = (uint8_t)(((val[1] & 0x0f) << 4) | (val[2] >> 2));
    out[o++] = (uint8_t)(((val[2] & 0x03) << 6) | val[3]);
  }

  if (i < len) {
    val[0] = b64_value(in[i]);
    val[1] = (i + 1 < len) ? b64_value(in[i + 1]) : -1;
    val[2] = (i + 2 < len) ? b64_value(in[i + 2]) : -1;
    if (val[0] < 0 || val[1] < 0) return EH_ERR_PARSE;
    if (o + 1 > out_cap) return EH_ERR_BUFFER_TOO_SMALL;
    out[o++] = (uint8_t)((val[0] << 2) | (val[1] >> 4));
    if (val[2] >= 0) {
      if (o + 1 > out_cap) return EH_ERR_BUFFER_TOO_SMALL;
      out[o++] = (uint8_t)(((val[1] & 0x0f) << 4) | (val[2] >> 2));
    }
  }

  *out_len = o;
  return EH_OK;
}

static const char B64URL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

eh_status_t eh_b64url_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap, size_t *out_len) {
  size_t i;
  size_t o = 0;

  if (!in || !out || !out_len) return EH_ERR_INVALID_ARG;
  if (out_cap == 0) return EH_ERR_BUFFER_TOO_SMALL;

  for (i = 0; i + 2 < in_len; i += 3) {
    if (o + 4 >= out_cap) return EH_ERR_BUFFER_TOO_SMALL;
    out[o++] = B64URL[in[i] >> 2];
    out[o++] = B64URL[((in[i] & 0x03) << 4) | (in[i + 1] >> 4)];
    out[o++] = B64URL[((in[i + 1] & 0x0f) << 2) | (in[i + 2] >> 6)];
    out[o++] = B64URL[in[i + 2] & 0x3f];
  }

  if (i < in_len) {
    if (o + 4 >= out_cap) return EH_ERR_BUFFER_TOO_SMALL;
    out[o++] = B64URL[in[i] >> 2];
    if (i + 1 < in_len) {
      out[o++] = B64URL[((in[i] & 0x03) << 4) | (in[i + 1] >> 4)];
      out[o++] = B64URL[(in[i + 1] & 0x0f) << 2];
    } else {
      out[o++] = B64URL[(in[i] & 0x03) << 4];
    }
  }

  out[o] = '\0';
  *out_len = o;
  return EH_OK;
}
