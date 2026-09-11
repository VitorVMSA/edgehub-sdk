# Changelog

## 0.2.3 — 2026-09-11

- `ehc_user_t.on_download`: callback de check-in `downloads` (OTA). NULL = só log.
- `EHC_CTX_ATTR`: `g_ehc` em PSRAM via `ehc_user_config.h` (sem patch em `ehc_core.c`).
- Port ESP32: uma repetição se o transporte falhar; contexto HTTP na stack.
- `ehc_lock` reentrante; `ehc_tel_set_*` toma o lock (seguro entre tasks e a partir de callbacks).
- Requer **edgehub-device-sdk ≥ 1.2.3**.

## 0.2.2 — 2026-09-11

- Port ESP32: POST via `open`/`write`/`fetch_headers`/`read` para não descartar HTTP 401 (EdgeHub usa 401 de aplicação, sem `WWW-Authenticate`). Lê `Retry-After` no event de header.
- Requer **edgehub-device-sdk ≥ 1.2.3**.

## 0.2.1 — 2026-08-19

- `ehc_tel_reset()`: cache `filled=0` e pool de strings vazio (campos condicionais não contaminam a leitura seguinte). Getters intactos. Sem auto-reset no `enqueue`.
- `ehc_tel_queue_clear()`: esvazia a fila RAM após falha de flush (o store da aplicação reenvia os mesmos `rec_id`).
- Fila do client **só em RAM**: `telemetry_queue_persist = 0` no init; o bridge ignora `eh.tel.q`. HMAC (`ehc.hmac.secret`) e assets (`ehc.asset.*`) continuam no port.
- Requer **edgehub-device-sdk ≥ 1.2.2**. `ehc_tel_flush` continua a devolver só `ehc_status_t`.

## 0.2.0 — 2026-08-19

- `ehc_tel_enqueue(ts_iso)` / `ehc_tel_flush()` / `ehc_tel_queued()`: fila sem flush imediato e timestamp explícito (replay histórico). `ehc_tel_publish()` permanece enqueue(NULL)+flush.
- `ehc_tick()` só drena a fila se `telemetry_manual_only == 0`.
- `ehc_user_t.telemetry_batch_max` (0 = `EHC_TELEMETRY_BATCH_MAX`) para `readings[]` sem recompilar a `.a`.
- Docs: mapa NVS legado EcoCircuito; compile fontes do device SDK com `-DEH_MAX_*` (sem `.a` 768). Requer **edgehub-device-sdk ≥ 1.2.1**.

## 0.1.0 — 2026-08-19

- Primeira versão da camada de integração portável (`edgehub-client-sdk`).
- Máquina de prontidão `UNCONFIGURED` → `MINIMAL` → `FULL` → `ACTIVE`.
- Tabelas estáticas de telemetria (JSON aninhado via `eh_values_add_raw`), check-in keys e comandos.
- Ports: Linux (OpenSSL + timeouts), ESP32 (`esp_http_client` + NVS) e `host_stub` para testes.
- Testes Unity/CTest no host. Requer `edgehub-device-sdk` ≥ **1.2.0** (ABI 2).
