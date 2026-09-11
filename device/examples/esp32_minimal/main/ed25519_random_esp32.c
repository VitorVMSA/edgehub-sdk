#include "ed25519-randombytes-custom.h"

#include "esp_random.h"
#include <string.h>

void ed25519_randombytes_unsafe(void *p, size_t len)
{
    uint8_t *out = (uint8_t *)p;
    size_t i = 0;

    if (p == NULL || len == 0) {
        return;
    }

    while (i < len) {
        uint32_t r = esp_random();
        size_t chunk = len - i;
        if (chunk > sizeof(r)) {
            chunk = sizeof(r);
        }
        memcpy(out + i, &r, chunk);
        i += chunk;
    }
}
