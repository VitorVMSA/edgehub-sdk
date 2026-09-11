---
name: ehc-porting
description: Adds or switches EdgeHub Client SDK ports (Linux, ESP32, generic embedded MCU). Use when the user mentions port, HAL, ehc_port_t, Linux, Apalis, ESP32, STM32, embarcado, EHC_PORT, or a new HTTP/storage backend.
---

# Portar o EdgeHub Client SDK

O protocolo (`device/`, ABI 2) e o client (`src/`, `include/`) são **iguais** em todos os alvos. Só muda o `ehc_port_t` e o `ed25519_random*`.

## Escolher o alvo

| Pedido | Fazer |
|--------|--------|
| Linux / Apalis | Ligar `port/linux`. CMake host: `-DEHC_PORT=linux`. Demo: `examples/linux_demo`. |
| ESP32 | `port/esp32` + IDF `examples/esp32_demo`. Não usar o CMake host. |
| Outro MCU | Copiar `port/embedded/` e preencher stubs. Demo de arranque: `examples/embedded_stub`. |
| Testes | Manter `port/host_stub` e `.\scripts\test_host.ps1 -DeviceSdk`. |

## HAL (`include/edgehub_client/ehc_port.h`)

Preencher:

- `http_post` — nonzero = transporte OK **incluindo HTTP 4xx/5xx**; preencher `http_status` e `Retry-After`
- `storage_read` / `storage_write` — HMAC `ehc.hmac.secret` / `.prev`; assets `ehc.asset.*`; **não** persistir `eh.tel.q`
- `now_ms` — obrigatório
- `log`, `lock`/`unlock` — opcionais (`lock` se tick e I/O em tasks distintas)

API da app: `ehc_init` + `ehc_tick`. Tabelas em `user_template/` (não ligar esses `.c` à `.a`).

## Random Ed25519

| Build | Ficheiro |
|-------|----------|
| Host CMake | `device/platform/ed25519_random_host.c` (já no `edgehub_device`) |
| ESP32 | `examples/esp32_demo/main/ed25519_random_esp32.c` |
| MCU novo | `port/embedded/ed25519_random_embedded.c` (substituir por TRNG) |

Não compilar dois `ed25519_randombytes_unsafe` no mesmo binário.

## O que não fazer

- Não copiar `src/` por plataforma
- Não alterar ABI do device
- Não meter fallback `config.apply` nem NVS legado EcoCircuito no port
- Slave: `user.disable_telemetry = 1`, `metric_count = 0`; RAM via `EH_MAX_*` no CMake do firmware

Detalhe: `docs/PORTING.md`.
