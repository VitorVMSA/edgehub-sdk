# Changelog

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
