# Guia de integração — EdgeHub Device SDK

Quem integra o **umbrella** (`ehc_*`) deve começar em [`docs/README.md`](../../docs/README.md) (HTTP, HMAC, flags). Este guia é a API `eh_*` directa.

## 1. Dependências

- Biblioteca estática `libedgehub_device.a` (ou `libedgehub_device_esp32s3.a`)
- Header `edgehub_device.h` (mesma versão que o `.a`)
- Implementação de `http_post` (HTTPS/TLS — responsabilidade do integrador).
  Opcional: `http_post_ex` com `Retry-After` (qualquer status HTTP); a assinatura de `http_post` não muda.
- Hook `ed25519_randombytes_unsafe` no target embarcado (ESP32)

## 2. Boot

```c
#include "edgehub_device.h"

static eh_client_t s_client;

static int my_http_post(void *user_ctx, const char *url, const char *headers,
                        const uint8_t *body, size_t body_len,
                        uint8_t *resp, size_t resp_cap, size_t *resp_len,
                        int *http_status) {
    /* esp_http_client, curl, etc. */
    return 0; /* 0 = success */
}

static int my_telemetry_collect(void *user_ctx, eh_values_writer_t *writer) {
    char buf[1792];
    if (eh_values_begin(writer, buf, sizeof(buf)) != EH_OK) return 0;
    if (eh_values_add_str(writer, "type", "telemetry") != EH_OK) return 0;
    if (eh_values_add_num(writer, "temperature", 23.5) != EH_OK) return 0;
    if (eh_values_end(writer) != EH_OK) return 0;
    return 1;
}

void edgehub_start(uint64_t now_ms) {
    eh_config_t cfg = {0};
    eh_callbacks_t cb = {0};

    strncpy(cfg.base_url, "https://app.edgehub.com.br", sizeof(cfg.base_url) - 1);
    strncpy(cfg.tenant_slug, "my-tenant", sizeof(cfg.tenant_slug) - 1);
    strncpy(cfg.device_uid, "SERIAL123", sizeof(cfg.device_uid) - 1);
    cfg.enable_checkin = 1;
    cfg.enable_telemetry = 1;
    cfg.enable_commands = 1;
    cfg.checkin_on_keys_change = 1;
    cfg.checkin_interval_ms = 43200000; /* 12 h */

    cb.http_post = my_http_post;
    cb.telemetry_collect = my_telemetry_collect;

    if (eh_lib_abi_check() != EH_OK) {
        /* header/.a mismatch — abort (inline: sizeof do firmware vs lib) */
        return;
    }

    eh_client_init(&s_client, &cfg, &cb, now_ms);
}

void edgehub_loop(uint64_t now_ms) {
    eh_client_tick(&s_client, now_ms);
}
```

`ts` da telemetria: se o firmware não chamar `eh_client_set_telemetry_ts`, a lib preenche ISO-8601 UTC a partir de `now_ms` (body e HMAC). **`now_ms` tem de ser epoch Unix** (milissegundos). Relógio de uptime (`esp_timer` desde o boot) produz `ts` 1970 → o servidor rejeita com `TELEMETRY_INVALID_TS` mesmo com HMAC correcto. O mesmo vale para `eh_poll_downlink`. Tests host podem usar `now_ms` sintético pequeno.

Lote / fila offline: `cfg.telemetry_batch_max` (0/1 = corpo legado `ts`/`values`; >1 envia `readings[]`). `eh_client_enqueue_reading` + `eh_client_flush_telemetry` drenam a fila; falha HTTP **não** descarta a amostra. `eh_client_telemetry_queue_clear` esvazia a fila RAM. Persistência `eh.tel.q` só corre se `cfg.telemetry_queue_persist != 0` (sem malloc do blob quando está a 0). Quem usa o device SDK sozinho e quer o comportamento offline de 1.2.1 deve pôr `telemetry_queue_persist = 1` — `memset` deixa o campo a 0. Body e HMAC de lote usam heap (`EH_MAX_BATCH_PAYLOAD`).

Números em `eh_values_add_num` seguem `JSON.stringify` (1.2.5). O servidor HMAC-a o body parseado+restringify; `%.15g` (`1e-05`) invalida o HMAC do lote.

Cada slot da fila tem `EH_MAX_TELEMETRY_QUEUE_VALUES` bytes (default **768**). Se o JSON de `values` passar disso, compile as fontes com `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096` (e, para controlar RAM, `-DEH_MAX_TELEMETRY_QUEUE=8`). O `.a` pré-compilado com 768 não serve — o define entra no tamanho de `eh_client_t`. Desde 1.2.1 o SDK **não** compila nem exporta `vendor/cjson` (parser interno `eh_json.c`).

HTTP: implementar `http_post_ex` (mesmo contrato que `http_post` + `uint32_t *retry_after_sec`) e deixar `http_post` como fallback ou omitir. A lib honra `Retry-After` em qualquer status. Check-in HTTP 401 `CHECKIN_DEVICE_IDENTITY_NOT_REGISTERED` aplica backoff 300 s se o port não enviar `Retry-After`. HTTP 401 `CHECKIN_DEVICE_CREDENTIAL_REVOKED` / `CHECKIN_DEVICE_CREDENTIAL_REQUIRED` arma re-provisionamento (como os equivalentes de telemetria). O port deve devolver o body também em 4xx (EdgeHub usa 401 de aplicação, sem `WWW-Authenticate`); `esp_http_client_perform` no ESP-IDF descarta esse caso. `identityChallenge` no body de 4xx ainda pode emitir HMAC e o check-in conta como sucesso.

`eh_client_tick`: falha de check-in **não** impede a telemetria do mesmo tick (o backoff aplica-se no tick seguinte). `eh_lib_abi_check()` é inline no header e compara `sizeof` do firmware com a lib ligada (`eh_lib_abi_check_impl`).

## 3. Check-in keys (MDM asset versions)

Registar no callback `checkin_keys_collect` (mesmo padrão `eh_values_add_*`):

```c
static int my_checkin_keys(void *user_ctx, eh_values_writer_t *w) {
    char buf[512];
    eh_values_begin(w, buf, sizeof(buf));
    eh_values_add_str(w, "firmware", "1.0.0");
    eh_values_add_str(w, "boardModel", "ECO-ESP32S3");
    eh_values_end(w);
    return 1;
}
```

## 4. Comandos e configuração remota

| Callback | Quando |
|----------|--------|
| `on_command` | Comando downlink recebido na resposta da telemetria |
| `on_checkin_download` | OTA/firmware oferecido pelo MDM |
| `on_checkin_configuration` | JSON de configuração por asset key |
| `store_read` / `store_write` | Persistência HMAC/secrets (opcional) |

### Comandos internos EdgeHub

O SDK trata comandos internos reservados antes de `on_command`:

| `commandKey` | Comportamento |
|--------------|---------------|
| `device.force_checkin` | Executa `eh_client_force_checkin` imediatamente |

Esse comando usa payload `{}`, não é repassado à aplicação e segue a confirmação
automática normal quando `enable_auto_confirm_commands` está ativo. A entrega ocorre em
`pendingCommands` na próxima resposta de telemetria.

## 5. Forçar envio

```c
eh_client_force_checkin(&s_client, now_ms);
eh_client_force_telemetry(&s_client, now_ms);
/* ou: enqueue + flush (lote / offline) */
eh_client_set_telemetry_batch_max(&s_client, 8);
eh_client_enqueue_reading(&s_client, NULL, now_ms);
eh_client_flush_telemetry(&s_client, now_ms);
```

## 6. ESP-IDF (prebuilt)

No `CMakeLists.txt` do componente consumidor:

```cmake
include($ENV{EDGEHUB_SDK_ROOT}/cmake/edgehub_device.cmake)
edgehub_device_use_prebuilt(
    $ENV{EDGEHUB_SDK_ROOT}/lib/esp32s3/libedgehub_device.a
    $ENV{EDGEHUB_SDK_ROOT}/include)
```

Ou copiar `lib/` + `include/` para `components/edgehub_device/` (ver EcoCircuito).

## 7. Requisitos operacionais

- Task dedicada recomendada: `eh_client_tick` a cada ~200 ms
- Stack TLS: ~32768 words PSRAM no ESP32 (HTTP dentro de `http_post`)
- `device_uid` deve coincidir com registo MDM
