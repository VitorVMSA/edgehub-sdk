# Progress

> Estado funcional — umbrella **0.3.2**. Device **1.2.6** ABI 2. Inclui tag `v0.3.1` (HMAC JSON) + ajustes ESP32 do slave. Testes host: correr `.\scripts\test_host.ps1 -DeviceSdk`.

## Linha de versões (histórico)

| Versão | Estado | Notas |
|--------|--------|-------|
| 0.1.0 | Tag anterior | Primeira camada portável; device irmão ≥ 1.2.0 |
| 0.2.1 | Tag | Fila RAM; `ehc_tel_reset` / `queue_clear` |
| 0.2.2 | Tag | Port ESP32 POST `open/write/fetch_headers/read` (HTTP 401) |
| 0.2.3 | Tag | `on_download`, `EHC_CTX_ATTR`, lock reentrante |
| 0.3.0 | Tag `v0.3.0` | Device in-tree 1.2.4; `disable_*`; lacunas EdgeHub |
| 0.3.1 | Tag `v0.3.1` | Device 1.2.5; números JSON = `JSON.stringify` (HMAC ingest) |
| **0.3.2** | Código | Device 1.2.6; `now_ms` Unix ESP32; heap check-in/issue/poll; `ehc_poll_downlink` |

## Funcional

| Área | Estado | Notas |
|------|--------|-------|
| Device in-tree `device/` 1.2.6 ABI 2 | ✅ Código | JSON.stringify + issue/poll malloc pequeno; challenge stash 512 B |
| HMAC ingest `\|x\|<1e-4` | ✅ Testes | `eh_test_hmac_golden` `1e-5` = `0.00001` |
| Client `src/` / `include/` | ✅ Código | `EHC_LIB_VERSION` 0.3.2; `ehc_poll_downlink` |
| `poll_commands` | ✅ Testes + campo | `ehc_tick` re-lê `now_ms` antes do POST |
| Port ESP32 `now_ms` | ✅ Código | `gettimeofday` + HTTP `Date`; SNTP = firmware |
| Diagnóstico ESP32 | ✅ Código | `last_http_op` / `last_cmd` |
| `disable_*` + all-off | ✅ Testes | três a 1 → `EHC_ECONFIG` |
| Host Unity/CTest | ✅ | correr após esta entrega |
| Releases `releases/v0.3.2/` | ✅ | SYNC; tarball gitignored |
| Slave EcoCircuito | ✅ Campo (0.3.1-local) | `third_party/`; `checkin_ok` + `downlink_poll_ok` |

## Issues conhecidos / abertos

| ID | Severidade | Descrição |
|----|------------|-----------|
| EH-OTA-GET | Info | Port só `http_post`; GET `downloadUrl` fica em `on_download` |
| EH-ESP32-DEMO | Info | `device/examples/esp32_minimal` `EH_SDK_ROOT=../..` é path de exemplo antigo |
| PP-CMD-POLL | Follow-up | Rota pública só-comandos no pixel-perfect — fora de âmbito |

Fechados: EH-CMD-TEL, EH-HMAC-PREV, EH-IDENT-FLAG, EH-INTERVAL-API, EH-OTA-CKSUM, EH-CMD-CAP, EH-ALL-OFF, FW-SUB-01 (slave), EH-ESP32-TS (1970 → `TELEMETRY_INVALID_TS`), EH-ESP32-HEAP (check-in+issue overlap).

## Próximos passos

1. Tag/push `v0.3.2` quando pedido
2. Apalis: `port/linux`
