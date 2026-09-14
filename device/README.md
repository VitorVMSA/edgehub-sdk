# EdgeHub Device SDK

Biblioteca C compilada (caixa preta) para integração de dispositivos embarcados com a plataforma EdgeHub MDM/Telemetry.

Versão actual: **1.2.5** (ABI 2).

## Conteúdo do release

```
include/edgehub_device.h     # API pública única
lib/libedgehub_device.a      # Biblioteca estática (por target)
cmake/edgehub_device.cmake   # Helper ESP-IDF add_prebuilt_library
releases/v1.2.3/            # release_notes, SYNC, MANIFEST
```

## Integração mínima

1. Alocar `eh_client_t` e preencher `eh_config_t`.
2. Registar `eh_callbacks_t` (mínimo: `http_post`).
3. Chamar `eh_lib_abi_check()` no boot.
4. Loop: `eh_client_tick(&cli, now_ms)`.
5. Telemetria: no callback `telemetry_collect`, usar `eh_values_begin` / `eh_values_add_*` / `eh_values_end`. Objectos aninhados: construir num writer de scratch e injectar com `eh_values_add_raw` (desde 1.2.0).

## Testes (host)

```powershell
cmake -S . -B build/tests -DEH_BUILD_TESTS=ON
cmake --build build/tests --config Release
ctest --test-dir build/tests -C Release --output-on-failure
```

Ver [docs/INTEGRATION.md](docs/INTEGRATION.md).

## Build (desenvolvedores SDK)

### Host (CI / simulador)

```powershell
.\scripts\build_host.ps1
# dist/host/libedgehub_device.a
```

### ESP32-S3

Requer ESP-IDF v5.x exportado:

```powershell
.\scripts\build_esp32s3.ps1
# dist/esp32s3/libedgehub_device.a
```

### Release tarball

```powershell
.\scripts\package_release.ps1 -Target esp32s3 -Version 1.0.0
```

## Plataforma: Ed25519 RNG

Com `ED25519_CUSTOMRANDOM`, o integrador deve fornecer:

```c
void ed25519_randombytes_unsafe(void *p, size_t len);
```

- Host/simulador: incluído em `platform/ed25519_random_host.c` (build SDK).
- ESP32: ver `examples/esp32_minimal/ed25519_random_esp32.c`.

## Versão e ABI

Header e `.a` devem ser da **mesma versão** (`EH_LIB_VERSION_*`). Use `eh_lib_abi_check()` para validar struct sizes.

`EH_LIB_ABI_VERSION` é **2** desde 1.1.0 (`eh_config_t` ganhou `telemetry_batch_max` / `telemetry_queue_capacity` / `telemetry_queue_persist`; `eh_client_t` inclui a fila de leituras). Firmware tem de recompilar com o mesmo header. A assinatura de `http_post` não muda; `http_post_ex` continua opcional no fim de `eh_callbacks_t`. Fila `eh.tel.q` só persiste com `telemetry_queue_persist != 0`.

O parser JSON da lib é `eh_json.c`. **Não** se compila `vendor/cjson` (1.2.1+) — evita colisão com o componente `cJson` do firmware ao ligar fontes.

Payloads maiores que o default **768** bytes (`EH_MAX_TELEMETRY_QUEUE_VALUES`) exigem **compilar fontes** com `-D`, por exemplo `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096` e `-DEH_MAX_TELEMETRY_QUEUE=8` (~32 KiB de fila vs ~12 KiB). O `.a` pré-compilado com 768 **não** serve; alterar o define muda o tamanho de `eh_client_t`, por isso header e objectos têm de nascer no mesmo compile.

## Licença

Proprietária EdgeHub / EcoCircuito — ver LICENSE.
