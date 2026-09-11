# Progress

> Estado funcional — umbrella **0.3.0** (tag `v0.3.0`). Testes host 2026-09-11: **15/15**.

## Linha de versões (histórico)

| Versão | Estado | Notas |
|--------|--------|-------|
| 0.1.0 | Tag anterior | Primeira camada portável; device irmão ≥ 1.2.0 |
| 0.2.1 | Tag | Fila RAM; `ehc_tel_reset` / `queue_clear` |
| 0.2.2 | Tag | Port ESP32 POST `open/write/fetch_headers/read` (HTTP 401) |
| 0.2.3 | Código | `on_download`, `EHC_CTX_ATTR`, lock reentrante |
| **0.3.0** | Tag `v0.3.0` | Device in-tree 1.2.4; `disable_*`; lacunas EdgeHub fechadas nesta lib |

Firmware EcoCircuito ainda em client **0.2.2** + device **1.2.3** (dois submódulos).

## Funcional

| Área | Estado | Notas |
|------|--------|-------|
| Device in-tree `device/` 1.2.4 ABI 2 | ✅ Código | `eh_client_t` + `credential_expires_at_ms`; header+.a juntos |
| Client `src/` / `include/` | ✅ Código | `EHC_LIB_VERSION` 0.3.0 |
| `disable_*` + all-off | ✅ Testes | três a 1 → `EHC_ECONFIG` |
| `poll_commands` | ✅ Testes | slave POST `{values:{}}`; sem flag = zero telemetria |
| HMAC previous / expiresAt | ✅ Testes | `eh_test_hmac_golden`, `eh_test_checkin` |
| PKCS#8 auto identity | ✅ Testes | `test_features` |
| Wrappers runtime | ✅ Testes | `ehc_set_*` / `ehc_get_device_status` |
| Check-in offer opcional | ✅ Testes | checksum/source vazios |
| Port ESP32 / Linux / embedded | ✅ | inalterado nesta entrega |
| Host Unity/CTest | ✅ | 15/15 |
| Releases `releases/v0.3.0/` | ✅ | SYNC; tarball gitignored |

## Issues conhecidos / abertos

| ID | Severidade | Descrição |
|----|------------|-----------|
| FW-SUB-01 | Follow-up | Firmware ainda com dois submódulos; mapa em `SYNC.txt` |
| EH-OTA-GET | Info | Port só `http_post`; GET `downloadUrl` fica em `on_download` (documentado; fora desta lib) |
| EH-ESP32-DEMO | Info | `device/examples/esp32_minimal` `EH_SDK_ROOT=../..` é path de exemplo antigo |
| PP-CMD-POLL | Follow-up | Rota pública só-comandos no pixel-perfect — fora de âmbito 0.3.0 |

Fechados nesta entrega: EH-CMD-TEL (opt-in heartbeat), EH-HMAC-PREV, EH-IDENT-FLAG, EH-INTERVAL-API, EH-OTA-CKSUM, EH-CMD-CAP (log), EH-ALL-OFF.

## Próximos passos

1. Migrar EcoCircuito master/slave para um submodule
2. Apalis: `port/linux`
