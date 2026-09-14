# Changelog

## 0.3.1 — 2026-09-13

- Device SDK **1.2.5** (ABI 2 inalterada): números JSON no formato de `JSON.stringify` (ECMA-262 NumberToString). Corrige HMAC 401 `TELEMETRY_DEVICE_CREDENTIAL_INVALID` quando o lote tem `|x|<1e-4` (`1e-05` vs `0.00001`).
- Testes host: `eh_test_values_writer` (paridade stringify) e `eh_test_hmac_golden` (`1e-5`).

## 0.3.0 — 2026-09-11

- Device SDK **1.2.4** (ABI 2) in-tree em `device/`. Um único repositório/submodule para todos os firmwares.
- `ehc_user_t.disable_telemetry` / `disable_commands` / `disable_checkin`: `memset` 0 = master actual (tudo ligado). Slave: `disable_telemetry=1` e `metric_count=0` — sem POST `/device/telemetry`; `ehc_tel_*` devolve `EHC_ENOTREADY`. Os três `disable_*` a 1 → `EHC_ECONFIG`.
- Comandos: downlink só em `pendingCommands` da telemetria; `executed` já existia. Opt-in `poll_commands` (slave) faz POST `{values:{}}` no mesmo ingest. HMAC: um retry com `secret_previous` em 401 INVALID; `expiresAt` arma reprovision. PKCS#8 auto-liga credential/issue. Wrappers `ehc_set_checkin_interval` / `ehc_set_telemetry_batch_max` / `ehc_set_telemetry_interval` / `ehc_get_device_status`. Check-in: checksum/source opcionais no offer.
- CMake: `EHC_DEVICE_SDK_ROOT` default `${repo}/device`; `EHC_TEST_DEVICE_SDK` default ON. `scripts/sync_device_sdk.ps1` deprecado.
- `EHC_PORT=host|linux|embedded|esp32`; template `port/embedded/` + `examples/embedded_stub` para MCU sem IDF.
- Colaboração com agentes: `AI-WORKFLOW.md`, `memory-bank/` (contexto + histórico), playbook em `docs/cursor-agents-playbook.md`.

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
