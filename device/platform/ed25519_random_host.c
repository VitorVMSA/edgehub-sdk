#include "../src/internal/ed25519_donna/ed25519-randombytes-custom.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ed25519_randombytes_unsafe(void *p, size_t len)
{
    uint8_t *out = (uint8_t *)p;
    size_t i;

    if (p == NULL || len == 0) {
        return;
    }

    for (i = 0; i < len; i++) {
        out[i] = (uint8_t)(rand() & 0xFF);
    }
}
