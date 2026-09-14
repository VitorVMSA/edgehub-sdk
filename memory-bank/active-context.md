# Active Context

> Última atualização: 2026-09-13

## Foco actual

- Umbrella **0.3.1** / device **1.2.5** (ABI 2): números JSON = `JSON.stringify` para HMAC do ingest.
- Tag `v0.3.1`. Firmware EcoCircuito aponta a este submodule.

## Decisões recentes

| Data | Decisão |
|------|---------|
| 2026-09-13 | Printer no device (`eh_json_print_number`), não no glue EcoCircuito nem no servidor. |
| 2026-09-13 | Shortest round-trip + regras ES (`-6<n≤21` decimal). `%.17g` sozinho gera dígitos a mais e falha o HMAC. |
| 2026-09-11 | Fundir device **para dentro** de `edgehub-client-sdk`, não o contrário nem repo novo. |
| 2026-09-11 | Flags `disable_*` (memset 0 = master ligado). Slave: `disable_telemetry=1`. |
| 2026-09-11 | Glue EcoCircuito (`config.apply`, NVS legado) **não** entra nesta lib. |

## WIP

- Apalis: ligar `port/linux` + submodule.
- Follow-up pixel-perfect: rota pública só-comandos (fora de âmbito).

## Histórico recente (esta linha de trabalho)

1. **0.3.1 HMAC números** — `eh_json_print_number`; testes `1e-5` vs Node; tag `v0.3.1`.
2. **Unificar 0.3.0** — `device/` 1.2.4 in-tree, flags `disable_*`.
3. Ports Linux vs embarcado — `port/embedded/` + CMake `EHC_PORT`.
4. **Lacunas EdgeHub** — `poll_commands`, HMAC previous + `expiresAt`, PKCS#8 auto identity.
