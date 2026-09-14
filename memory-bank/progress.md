# Progress

> Estado funcional — umbrella **0.3.1** (tag `v0.3.1`). Testes host 2026-09-13: **15/15**.

## Linha de versões (histórico)

| Versão | Estado | Notas |
|--------|--------|-------|
| 0.1.0 | Tag anterior | Primeira camada portável; device irmão ≥ 1.2.0 |
| 0.2.1 | Tag | Fila RAM; `ehc_tel_reset` / `queue_clear` |
| 0.2.2 | Tag | Port ESP32 POST `open/write/fetch_headers/read` (HTTP 401) |
| 0.2.3 | Tag | `on_download`, `EHC_CTX_ATTR`, lock reentrante |
| 0.3.0 | Tag `v0.3.0` | Device in-tree 1.2.4; `disable_*`; lacunas EdgeHub |
| **0.3.1** | Tag `v0.3.1` | Device 1.2.5; números JSON = `JSON.stringify` (HMAC ingest) |

## Funcional

| Área | Estado | Notas |
|------|--------|-------|
| Device in-tree `device/` 1.2.5 ABI 2 | ✅ Código | `eh_json_print_number` + `eh_values_add_num` |
| HMAC ingest `\|x\|<1e-4` | ✅ Testes | `eh_test_hmac_golden` `1e-5` = `0.00001` |
| Client `src/` / `include/` | ✅ Código | `EHC_LIB_VERSION` 0.3.1 |
| `disable_*` + all-off | ✅ Testes | três a 1 → `EHC_ECONFIG` |
| HMAC previous / expiresAt | ✅ Testes | inalterado nesta entrega |
| Host Unity/CTest | ✅ | 15/15 |
| Releases `releases/v0.3.1/` | ✅ | SYNC; tarball gitignored |

## Issues conhecidos / abertos

| ID | Severidade | Descrição |
|----|------------|-----------|
| EH-OTA-GET | Info | Port só `http_post`; GET `downloadUrl` fica em `on_download` |
| EH-ESP32-DEMO | Info | `device/examples/esp32_minimal` `EH_SDK_ROOT=../..` é path de exemplo antigo |
| PP-CMD-POLL | Follow-up | Rota pública só-comandos no pixel-perfect — fora de âmbito |

## Próximos passos

1. Consumo EcoCircuito na tag `v0.3.1`
2. Apalis: `port/linux`
