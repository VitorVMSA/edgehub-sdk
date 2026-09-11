# AGENTS.md — EdgeHub Client SDK

Entrada para agentes neste repositório. Camada C99 portável sobre `edgehub-device-sdk` ≥ **1.2.3**.

| Área | Caminho |
|------|---------|
| Versão | `VERSION` (`0.2.3`), `EHC_LIB_VERSION` em `include/edgehub_client/ehc.h` |
| API | `include/edgehub_client/` |
| Fontes | `src/` |
| Port ESP32 | `port/esp32/` (POST `open/write/fetch_headers/read` — não descartar HTTP 401) |
| Release | `releases/v0.2.3/` (`release_notes_023.txt`, `SYNC.txt`, `MANIFEST.txt`) |
| Changelog | `CHANGELOG.md` |
| Integração | `docs/INTEGRATION.md`, `docs/PORTING.md`, `docs/TESTING.md` |
| Device SDK | irmão `../edgehub-device-sdk` |
| Firmware EcoCircuito | irmão `../ecocircuito-maq_v3.0_refatorado` — `scripts/sync_edgehub_sources.ps1` |

## Comandos

```powershell
.\scripts\test_host.ps1
.\scripts\package_release.ps1 -Version 0.2.3
```

Após sync para o firmware: `u->on_download = ehc_user_on_download`; `EHC_CTX_ATTR` em `ehc_user_config.h` (PSRAM). Reaplicar só o fallback `config.apply` em `ehc_commands.c`.

Não commitar sem pedido explícito. Respostas ao utilizador: português.
