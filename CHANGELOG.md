# Changelog

## 0.1.0 — 2026-08-19

- Primeira versão da camada de integração portável (`edgehub-client-sdk`).
- Máquina de prontidão `UNCONFIGURED` → `MINIMAL` → `FULL` → `ACTIVE`.
- Tabelas estáticas de telemetria (JSON aninhado via `eh_values_add_raw`), check-in keys e comandos.
- Ports: Linux (OpenSSL + timeouts), ESP32 (`esp_http_client` + NVS) e `host_stub` para testes.
- Testes Unity/CTest no host. Requer `edgehub-device-sdk` ≥ **1.2.0** (ABI 2).
