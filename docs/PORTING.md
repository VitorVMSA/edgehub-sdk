# Porting

O HAL está em `include/edgehub_client/ehc_port.h`. Implemente:

| Callback | Notas |
|---|---|
| `http_post` | **nonzero = transporte OK**, mesmo em HTTP 4xx/5xx. Preencha `http_status` e, em 429, `retry_after_sec`. Use esta variante (`http_post_ex` no device SDK). |
| `storage_read` / `storage_write` | blobs opacos. Segredo HMAC em `ehc.hmac.secret` / `.prev`. Versões de asset em `ehc.asset.<key>`. |
| `now_ms` | relógio monotónico ou wall-clock em milissegundos. |
| `log` | opcional |
| `lock` / `unlock` | opcional; obrigatório se `ehc_tick` e HTTP/storage puderem correr em threads distintas |

## Linux (`port/linux`)

OpenSSL BIO com `SO_RCVTIMEO`/`SO_SNDTIMEO` (default 12 s). Storage = um ficheiro por chave num directório. `clock_gettime(CLOCK_REALTIME)`. Mutex pthread. `ehc_thread_start(period_ms)` arranca um loop de `ehc_tick`.

## ESP32 (`port/esp32`) — greenfield

`esp_http_client` + `esp_crt_bundle_attach` + keep-alive. NVS namespace `ehc` (chaves mapeadas para 15 caracteres via FNV-1a `k%08x`).

**Regra dura:** I/O de NVS só em tasks com stack em **RAM interna**. Se a task do client corre em PSRAM, compile com `-DEHC_PORT_ESP32_STORAGE_WORKER=1` — o port despacha read/write para um worker interno.

Compile as **fontes** do device SDK **1.2.1+** (já sem `vendor/cjson` nos `INCLUDE_DIRS`). O `.a` prebuilt com `EH_MAX_TELEMETRY_QUEUE_VALUES=768` **não** serve para payloads maiores — use `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096` (e `-DEH_MAX_TELEMETRY_QUEUE=8`) no mesmo compile que o header.

## Storage legado (EcoCircuito já provisionado)

**Não** use o NVS genérico `ehc` / `k%08x` num equipamento que já tem HMAC e versões de asset. Sem tradução, o segredo desaparece, o check-in volta a `pending` e as versões caem em `"0"` (re-download de toda a configuração MDM).

O conteúdo é string plana nos dois lados. O glue do firmware implementa `storage_read` / `storage_write`:

| Chave `ehc_*` | Destino legado |
|---|---|
| `ehc.hmac.secret` | `config_manager` / NVS `config_manager` / `edgehub_hmac_secret` |
| `ehc.hmac.secret.prev` | `edgehub_hmac_secret_prev` |
| `ehc.asset.<key>` | namespace `EdgeHubAssetVer`, chaves `eh_v_*` via `edgehub_asset_version_get/set` |

Identidade (`base_url`, UID, group, product, PKCS#8) → `EHC_IDENT_FN` sobre o `config_manager`, **não** storage do client.

I/O NVS continua nas tasks de RAM interna que já existem.

## Host stub (`port/host_stub`)

Servidor HTTP falso, relógio `ehc_test_clock_advance`, storage em memória com falha injectável. Só para testes.

## ABI

`ehc_init` chama `eh_lib_abi_check()` primeiro. Mismatch → `EHC_EABI`.
