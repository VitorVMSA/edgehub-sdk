# System Patterns

> Arquitetura resumida — detalhe em `docs/INTEGRATION.md`, `docs/PORTING.md`.

## Visão geral

```
edgehub-client-sdk 0.3.2
  device/          protocolo ABI 2 (1.2.6)
  src/ include/    camada produto (ehc_*)
  port/            HAL (http, storage, now, lock)
    host_stub/     testes
    linux/         OpenSSL + ficheiros
    esp32/         IDF + NVS + gettimeofday
    embedded/      stubs MCU
  user_template/   tabelas do produto (não na .a)
```

```mermaid
flowchart TB
  subgraph sdk [edgehub-client-sdk 0.3.2]
    device[device/ ABI2]
    client[src/ include/]
    port[port/]
    client --> device
    client --> port
  end
  app[Firmware ou processo Linux]
  app -->|"ehc_init + ehc_tick"| client
  app -->|"tabelas"| user[user_template]
```

## Padrão: um port por alvo

O client **não** se duplica. Troca-se `ehc_port_t` + `ed25519_random*`.

| Alvo | Port | Random |
|------|------|--------|
| Testes | `port/host_stub` | `device/platform/ed25519_random_host.c` |
| Linux | `port/linux` | host |
| ESP32 | `port/esp32` | `examples/esp32_demo/main/ed25519_random_esp32.c` |
| MCU | `port/embedded` | `port/embedded/ed25519_random_embedded.c` (TRNG) |

CMake host: `-DEHC_PORT=host|linux|embedded|esp32`. ESP32 real = projecto IDF `examples/esp32_demo`.

## Padrão: consumo firmware

Slave EcoCircuito: submodule `third_party/edgehub-client-sdk` (não `components/` — o `project()` host parte o configure IDF).

- Device: fontes `device/`
- Client: `src`, `include`, `port/esp32`
- Overlay EcoCircuito: `config.apply` / NVS legado / SNTP **fora** desta lib

Boot: `lib=1.2.6 client=0.3.2 abi=2`.

## Padrão: `now_ms` no ingest

`ts` ISO-8601 e HMAC saem de `now_ms`. Em produção ESP32 isso **é** epoch Unix (`gettimeofday`). Testes host podem usar relógio sintético. SNTP não pertence à lib.

## Padrão: features a menos

`memset(&user,0)` = master. Slave: `disable_telemetry=1`, `metric_count=0`, `poll_commands=1`. `ehc_init` recusa os três `disable_*` a 1. RAM (`EH_MAX_*`) no CMake do firmware.
