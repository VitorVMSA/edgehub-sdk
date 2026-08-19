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

## ESP32 (`port/esp32`)

`esp_http_client` + `esp_crt_bundle_attach` + keep-alive. NVS namespace `ehc` (chaves mapeadas para 15 caracteres via FNV-1a).

**Regra dura:** I/O de NVS só em tasks com stack em **RAM interna**. Se a task do client corre em PSRAM, compile com `-DEHC_PORT_ESP32_STORAGE_WORKER=1` — o port despacha read/write para um worker interno.

O `.a` prebuilt do device SDK tem de ser o **1.2.0** (função nova `eh_values_add_raw`). Regenerar quando migrar o firmware ESP32.

## Host stub (`port/host_stub`)

Servidor HTTP falso, relógio `ehc_test_clock_advance`, storage em memória com falha injectável. Só para testes.

## ABI

`ehc_init` chama `eh_lib_abi_check()` primeiro. Mismatch → `EHC_EABI`.
