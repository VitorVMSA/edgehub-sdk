#include <stddef.h>
#include <stdint.h>

/*
 * Ligar no firmware MCU em vez de device/platform/ed25519_random_host.c.
 * Substituir por TRNG (ex.: HAL_RNG). Este fallback é só para compilar o stub.
 */
void ed25519_randombytes_unsafe(void *p, size_t len)
{
  uint8_t *out = (uint8_t *)p;
  size_t i;
  static uint32_t s = 0xA5A5A5A5u;
  if (!p || len == 0) {
    return;
  }
  for (i = 0; i < len; i++) {
    s = s * 1664525u + 1013904223u;
    out[i] = (uint8_t)(s >> 16);
  }
}
