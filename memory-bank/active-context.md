# Active Context

> Última atualização: 2026-09-13

## Foco actual

- Umbrella **0.3.2** / device **1.2.6** (ABI 2): relógio Unix no port ESP32, heap de check-in/issue/poll, `ehc_poll_downlink()`, em cima da tag `v0.3.1` (números JSON = `JSON.stringify`).
- Firmware slave EcoCircuito consome `third_party/edgehub-client-sdk`. Validado em WROOM: `checkin_ok`, `downlink_poll_ok`, `command_executed`.

## Decisões recentes

| Data | Decisão |
|------|---------|
| 2026-09-13 | Base correcta = tag GitHub `v0.3.1`. Ajustes ESP32/heap/poll do slave entram como **0.3.2**. |
| 2026-09-13 | Printer no device (`eh_json_print_number`), não no glue EcoCircuito nem no servidor. |
| 2026-09-13 | Shortest round-trip + regras ES (`-6<n≤21` decimal). `%.17g` sozinho gera dígitos a mais e falha o HMAC. |
| 2026-09-12 | `now_ms` no ingest = epoch Unix. SNTP no firmware; port ESP32 = `gettimeofday` + bootstrap HTTP `Date`. Não recusar `now_ms` pequeno na lib (testes host). |
| 2026-09-12 | Check-in liberta buffers grandes **antes** de `credential/issue`. Poll/issue: malloc pequeno, não 8–12 KB na stack MDM. |
| 2026-09-12 | `ehc_tick` re-lê `ehc_now()` imediatamente antes do poll. Slave IDF: submodule em `third_party/`, não `components/`. |
| 2026-09-11 | Fundir device **para dentro** de `edgehub-client-sdk`. |
| 2026-09-11 | Flags `disable_*` (memset 0 = master ligado). Slave: `disable_telemetry=1`. |
| 2026-09-11 | Glue EcoCircuito (`config.apply`, NVS legado, SNTP) **não** entra nesta lib. |
| 2026-09-11 | Sem rota nova de poll de comandos. Heartbeat opt-in no ingest de telemetria. |

## WIP

- Tag `v0.3.2` / push: só com pedido explícito.
- Apalis: ligar `port/linux` + submodule.
- Follow-up pixel-perfect: rota pública só-comandos (fora de âmbito).

## Histórico recente (esta linha de trabalho)

1. **0.3.2 ESP32** — wall-clock `now_ms`, heap check-in/issue/poll, `ehc_poll_downlink`, sobre `v0.3.1`.
2. **0.3.1 HMAC números** — `eh_json_print_number`; testes `1e-5` vs Node; tag `v0.3.1`.
3. **Unificar 0.3.0** — `device/` 1.2.4 in-tree, flags `disable_*`, `releases/v0.3.0/`.
4. Ports Linux vs embarcado — `port/embedded/` + CMake `EHC_PORT`.
5. **Lacunas EdgeHub** — `poll_commands`, HMAC previous + `expiresAt`, PKCS#8 auto identity.
