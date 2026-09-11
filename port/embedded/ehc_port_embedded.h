#ifndef EHC_PORT_EMBEDDED_H
#define EHC_PORT_EMBEDDED_H

#include "edgehub_client/ehc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * HAL genérico (C99, sem POSIX/IDF). Copiar para o MCU e substituir os stubs:
 * HTTPS, flash, relógio, mutex, TRNG (`ed25519_random_embedded.c`).
 *
 * `http_post` devolve 0 (transporte falhou) até haver stack TLS.
 */
int ehc_port_embedded_init(ehc_port_t *out);
void ehc_port_embedded_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
