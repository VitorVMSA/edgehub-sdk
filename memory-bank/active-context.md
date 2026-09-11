# Active Context

> Última atualização: 2026-09-11

## Foco actual

- Umbrella **0.3.0**: `device/` in-tree (device SDK 1.2.4, ABI 2) + client na raiz. Tag `v0.3.0`.
- Lacunas EdgeHub vs 0.3.0 fechadas nesta lib (comandos, HMAC previous, identity, intervalos, OTA offer, all-off). Sem mudança no servidor.
- Firmware EcoCircuito **não** migrado nesta entrega (ainda dois submódulos v1.2.3 / v0.2.2).

## Decisões recentes

| Data | Decisão |
|------|---------|
| 2026-09-11 | Fundir device **para dentro** de `edgehub-client-sdk`, não o contrário nem repo novo. |
| 2026-09-11 | Flags `disable_*` (memset 0 = master ligado). Slave: `disable_telemetry=1`. |
| 2026-09-11 | Glue EcoCircuito (`config.apply`, NVS legado) **não** entra nesta lib. |
| 2026-09-11 | Import `device/` por cópia (sem subtree/commit). Testes device prefixo `eh_` no CTest. |
| 2026-09-11 | Troca Linux/embarcado = só o port; CMake `-DEHC_PORT=host\|linux\|embedded\|esp32`. |
| 2026-09-11 | Memory-bank + agentes + rules de entrega iguais ao padrão `AI-WORKFLOW.md`. |
| 2026-09-11 | Sem rota nova de poll de comandos. Heartbeat opt-in no ingest de telemetria. HAL continua só `http_post`. |

## WIP

- Follow-up firmware: um submodule `components/edgehub_sdk`; slave apaga `edgehub_mdm`.
- Apalis: ligar `port/linux` + submodule.
- Follow-up pixel-perfect: rota pública só-comandos (fora de âmbito 0.3.0).

## Histórico recente (esta linha de trabalho)

1. **Unificar 0.3.0** — `device/` 1.2.4 in-tree, flags `disable_*`, `VERSION`/docs/`releases/v0.3.0/SYNC.txt`.
2. Ports Linux vs embarcado — `port/embedded/` + CMake `EHC_PORT`.
3. Memory-bank + workflow + agentes.
4. **Lacunas EdgeHub** — `poll_commands`, HMAC previous + `expiresAt`, PKCS#8 auto identity, wrappers runtime, checksum/source opcionais, all-off `EHC_ECONFIG`. Testes host **15/15**.
