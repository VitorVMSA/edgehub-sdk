# EdgeHub Client SDK

Camada C99 portável. Inclui o protocolo EdgeHub device **1.2.6** (ABI 2) em `device/`. Trata sozinha de prontidão, check-in mínimo vs completo, telemetria (tabela + fila RAM + lote `readings[]`), comandos e flags `disable_*`.

Versão umbrella: **0.3.3** (`lib=1.2.6 client=0.3.3 abi=2`). A fila de telemetria é RAM; persistência e replay ficam na aplicação consumidora.

**Documentação:** [`docs/README.md`](docs/README.md) (HTTP, payloads, HMAC, ports, runbooks). Este README não substitui esse contrato.

## Layout

```
device/                   protocolo (ABI 2, 1.2.6)
include/edgehub_client/   API pública do client
src/                      núcleo do client
port/{esp32,linux,embedded,host_stub}/
user_template/            copiar para o firmware
examples/{linux_demo,esp32_demo,embedded_stub}/
tests/                    Unity + CTest
docs/                     contrato (HTTP, HMAC, ports, runbooks)
releases/v0.3.2/
```

CMake: `add_subdirectory(device)` (`EHC_DEVICE_SDK_ROOT` default).

## Build e testes (host)

```powershell
.\scripts\test_host.ps1 -DeviceSdk
```

```powershell
cmake -S . -B build -DEHC_BUILD_TESTS=ON -DEHC_TEST_DEVICE_SDK=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Unix: `scripts/test_host.sh`. Testes Unity do device in-tree: nomes `eh_test_*`.

## Integração rápida

1. Submodule deste repo no firmware (`releases/v0.3.2/SYNC.txt`).
2. Copiar `user_template/` e preencher as tabelas.
3. Ligar um port: `port/linux`, `port/esp32`, ou copiar `port/embedded/` (`-DEHC_PORT=linux|embedded|esp32`).
4. `ehc_init(&port, &user)` e `ehc_tick()`.

Slave: `user.disable_telemetry = 1` e `metric_count = 0`. Slave IDF: submodule em `third_party/`, **não** `components/`. Glue EcoCircuito (`config.apply`, NVS legado, SNTP) **não** entra nesta lib.

Agentes: `AGENTS.md`. Changelog: `CHANGELOG.md`. Docs: [`docs/README.md`](docs/README.md).
