# EdgeHub Client SDK

Camada C99 portável por cima do [`edgehub-device-sdk`](https://github.com) **≥ v1.2.2** (ABI 2). Trata sozinha de:

- **Prontidão** — não envia nada à rede sem `base_url`, `device_uid` e keys obrigatórias
- **Check-in** mínimo vs completo (`group_key` + `product_code`)
- **Telemetria** — tabela estática, JSON aninhado, push, getters, fila (`enqueue`/`flush`) e lote `readings[]`
- **Comandos** — dispatch por `commandKey`, argumentos tipados, `DEFER_AFTER_ACK`

Versão: **0.2.1**. Sem `malloc` nesta camada (o batch de 64 KiB do device SDK continua opt-in). A fila de telemetria do client é só RAM — persistência de pacotes fica na aplicação.

## Layout

```
include/edgehub_client/   API pública
src/                      núcleo
port/{esp32,linux,host_stub}/
user_template/            copiar para o firmware
examples/{linux_demo,esp32_demo}/
tests/                    Unity + CTest
```

O device SDK é um submodule em `third_party/edgehub-device-sdk` **ou** o irmão `../edgehub-device-sdk`. CMake procura os dois. `scripts/sync_device_sdk.ps1` cria a pasta local.

## Build e testes (host)

```powershell
cmake -S . -B build -DEHC_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Ou `scripts/test_host.ps1` / `scripts/test_host.sh`.

Os testes da lib C (device SDK) correm no mesmo `ctest` com `-DEHC_TEST_DEVICE_SDK=ON`.

## Integração rápida

1. Copiar `user_template/` para o projecto e preencher as tabelas.
2. Ligar um port (`port/linux` ou `port/esp32`).
3. `ehc_init(&port, &user)` e `ehc_tick()` no loop.

Ver `docs/INTEGRATION.md`, `docs/PORTING.md`, `docs/TESTING.md`.

A migração dos firmwares ESP32 EcoCircuito e Apalis **não** faz parte desta entrega.
