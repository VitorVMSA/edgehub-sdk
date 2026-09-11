#include "eh_hmac.h"

#include <stdio.h>
#include <string.h>

#include "eh_sha256.h"

static void xor_pad(uint8_t *out, const uint8_t *key, size_t key_len, uint8_t val) {
  size_t i;
  for (i = 0; i < 64; ++i) {
    out[i] = (i < key_len ? key[i] : 0) ^ val;
  }
}

eh_status_t eh_hmac_sha256_hex(const char *secret, const char *message, char out_hex[65]) {
  eh_sha256_ctx ctx;
  uint8_t key[64];
  uint8_t inner[64];
  uint8_t outer[64];
  uint8_t inner_hash[EH_SHA256_DIGEST_SIZE];
  uint8_t digest[EH_SHA256_DIGEST_SIZE];
  size_t secret_len;
  size_t message_len;
  size_t i;

  if (!secret || !message || !out_hex) return EH_ERR_INVALID_ARG;
  secret_len = strlen(secret);
  message_len = strlen(message);

  if (secret_len > 64) {
    eh_sha256_init(&ctx);
    eh_sha256_update(&ctx, (const uint8_t *)secret, secret_len);
    eh_sha256_final(&ctx, key);
    secret_len = EH_SHA256_DIGEST_SIZE;
    memset(key + secret_len, 0, 64 - secret_len);
  } else {
    memcpy(key, secret, secret_len);
    memset(key + secret_len, 0, 64 - secret_len);
  }

  xor_pad(inner, key, 64, 0x36);
  xor_pad(outer, key, 64, 0x5c);

  eh_sha256_init(&ctx);
  eh_sha256_update(&ctx, inner, 64);
  eh_sha256_update(&ctx, (const uint8_t *)message, message_len);
  eh_sha256_final(&ctx, inner_hash);

  eh_sha256_init(&ctx);
  eh_sha256_update(&ctx, outer, 64);
  eh_sha256_update(&ctx, inner_hash, EH_SHA256_DIGEST_SIZE);
  eh_sha256_final(&ctx, digest);

  for (i = 0; i < EH_SHA256_DIGEST_SIZE; ++i) {
    snprintf(out_hex + i * 2, 3, "%02x", digest[i]);
  }
  out_hex[64] = '\0';
  return EH_OK;
}
