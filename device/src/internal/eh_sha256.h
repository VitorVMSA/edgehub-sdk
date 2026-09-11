#ifndef EH_SHA256_H
#define EH_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define EH_SHA256_DIGEST_SIZE 32

typedef struct {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
} eh_sha256_ctx;

void eh_sha256_init(eh_sha256_ctx *ctx);
void eh_sha256_update(eh_sha256_ctx *ctx, const uint8_t *data, size_t len);
void eh_sha256_final(eh_sha256_ctx *ctx, uint8_t hash[EH_SHA256_DIGEST_SIZE]);

#endif
