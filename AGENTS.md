# AGENTS.md — EdgeHub Client SDK

Entrada para agentes neste repositório. Camada C99 portável sobre `edgehub-device-sdk` ≥ **1.2.3**.

| Área | Caminho |
|------|---------|
| Versão | `VERSION` (`0.2.2`), `EHC_LIB_VERSION` em `include/edgehub_client/ehc.h` |
| API | `include/edgehub_client/` |
| Fontes | `src/` |
| Port ESP32 | `port/esp32/` (POST `open/write/fetch_headers/read` — não descartar HTTP 401) |
| Release | `releases/v0.2.2/` (`release_notes_022.txt`, `SYNC.txt`, `MANIFEST.txt`) |
| Changelog | `CHANGELOG.md` |
| Integração | `docs/INTEGRATION.md`, `docs/PORTING.md`, `docs/TESTING.md` |
| Device SDK | irmão `../edgehub-device-sdk` |
| Firmware EcoCircuito | irmão `../ecocircuito-maq_v3.0_refatorado` — `scripts/sync_edgehub_sources.ps1` |

## Comandos

```powershell
.\scripts\test_host.ps1
.\scripts\package_release.ps1 -Version 0.2.2
```

Após sync para o firmware, reaplicar patches EcoCircuito (não pertencem a esta lib): `g_ehc` PSRAM, `ehc_user_on_download`, fallback `config.apply`.

Não commitar sem pedido explícito. Respostas ao utilizador: português.
