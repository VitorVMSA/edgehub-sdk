# AGENTS.md — EdgeHub Device SDK

Entrada para agentes neste repositório. Lib C (ABI 2) para MDM/Telemetry EdgeHub.

| Área | Caminho |
|------|---------|
| Versão | `VERSION` (`1.2.6`), header `EH_LIB_VERSION_*` / `EH_LIB_ABI_VERSION` **2** |
| API | `include/edgehub_device.h` |
| Fontes | `src/` + `src/internal/` |
| Release | `releases/v1.2.3/` (`release_notes_123.txt`, `SYNC.txt`, `MANIFEST.txt`) |
| Changelog | `CHANGELOG.md` |
| Documentação de integração | `../docs/` no umbrella (PROTOCOL, HMAC, INTEGRATION) |
| Firmware EcoCircuito | irmão `../ecocircuito-maq_v3.0_refatorado`

## Comandos

```powershell
cmake -S . -B build/tests -DEH_BUILD_TESTS=ON
cmake --build build/tests --config Release
ctest --test-dir build/tests -C Release --output-on-failure
.\scripts\package_release.ps1 -Target host -Version 1.2.6
```

Payloads EcoCircuito (~4 KiB): compilar **fontes** com `-DEH_MAX_TELEMETRY_QUEUE=8` e `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096`. O `.a` default 768 não serve.

Não commitar sem pedido explícito. Respostas ao utilizador: português.
