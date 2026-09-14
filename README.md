# EdgeHub Client SDK

Camada C99 portável. Inclui o protocolo [`edgehub-device-sdk`](https://github.com/VitorVMSA/edgehub-device-sdk) **1.2.5** (ABI 2) em `device/`. Trata sozinha de:

- **Prontidão** — não envia nada à rede sem `base_url`, `device_uid` e keys obrigatórias
- **Check-in** mínimo vs completo (`group_key` + `product_code`)
- **Telemetria** — tabela estática, JSON aninhado, push, getters, fila (`enqueue`/`flush`) e lote `readings[]`
- **Comandos** — dispatch por `commandKey`, argumentos tipados, `DEFER_AFTER_ACK`
- **Features a menos** — `disable_telemetry` / `disable_commands` / `disable_checkin` (`memset` 0 = tudo ligado)

Versão umbrella: **0.3.1** (`lib=1.2.5 client=0.3.1 abi=2`). Sem `malloc` nesta camada (o batch de 64 KiB do device SDK continua opt-in). A fila de telemetria do client é só RAM — persistência de pacotes fica na aplicação. Notas e manifesto de sync: `releases/v0.3.1/`.

## Layout

```
device/                   protocolo (ABI 2, 1.2.5)
include/edgehub_client/   API pública do client
src/                      núcleo do client
port/{esp32,linux,embedded,host_stub}/
user_template/            copiar para o firmware
examples/{linux_demo,esp32_demo,embedded_stub}/
tests/                    Unity + CTest
releases/v0.3.1/          release_notes, SYNC, MANIFEST
```

O CMake faz `add_subdirectory(device)` (`EHC_DEVICE_SDK_ROOT` default). Não é preciso o irmão `../edgehub-device-sdk` nem `third_party/`.

## Build e testes (host)

```powershell
.\scripts\test_host.ps1 -DeviceSdk
```

Ou:

```powershell
cmake -S . -B build -DEHC_BUILD_TESTS=ON -DEHC_TEST_DEVICE_SDK=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Em Unix: `scripts/test_host.sh`. Os testes Unity do device in-tree correm no mesmo `ctest` (nomes `eh_test_*`).

## Integração rápida

1. Submodule deste repo no firmware (ver `releases/v0.3.1/SYNC.txt`).
2. Copiar `user_template/` para o projecto e preencher as tabelas.
3. Ligar um port: `port/linux`, `port/esp32`, ou copiar `port/embedded/` para outro MCU (`-DEHC_PORT=linux|embedded|esp32`).
4. `ehc_init(&port, &user)` e `ehc_tick()` no loop.

Slave sem telemetria: `user.disable_telemetry = 1` e `metric_count = 0`. RAM (`EH_MAX_*`, `EHC_TEL_STR_MAX`) continua a ser CMake do firmware.

Ver `docs/INTEGRATION.md`, `docs/PORTING.md`, `docs/TESTING.md`. Índice: `docs/README.md`.

Agentes e memória entre sessões: `AGENTS.md`, `AI-WORKFLOW.md`, `memory-bank/`.

A migração dos firmwares ESP32 EcoCircuito e Apalis **não** faz parte desta entrega. Glue EcoCircuito (`config.apply` fallback, NVS legado) **não** entra nesta lib.
