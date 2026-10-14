# AGENTS.md — EdgeHub Client SDK

Entrada única para agentes **neste git**. Leia isto antes de propor ou implementar. Estrutura Cursor: [`AI-WORKFLOW.md`](AI-WORKFLOW.md).

**Contrato da biblioteca (HTTP, payloads, ports, runbooks):** [`docs/README.md`](docs/README.md) — este git. Quem clona o repo **não** precisa de Obsidian. Runtime Cursor (`AGENTS.md`, `.cursor/`) fica aqui. Memória de sessão: vault `90 - Agent Workspace/EdgeHub/Client SDK/`. Contrato público: `docs/`.

Umbrella **0.3.3**: protocolo in-tree (`device/` = edgehub-device-sdk **1.2.6**, ABI 2) + camada de produto.

| Área | Caminho | Docs |
|------|---------|------|
| Versão | `VERSION`, `EHC_LIB_VERSION` em `include/edgehub_client/ehc.h` | `CHANGELOG.md` |
| Device | `device/` (1.2.6, ABI 2) | [docs/PROTOCOL.md](docs/PROTOCOL.md) · `device/docs/INTEGRATION.md` |
| Client | `include/edgehub_client/`, `src/` | [docs/API.md](docs/API.md) · [docs/INTEGRATION.md](docs/INTEGRATION.md) |
| Ports | `port/{host_stub,linux,esp32,embedded}/` | [docs/PORTING.md](docs/PORTING.md) |
| Template produto | `user_template/` | [docs/INTEGRATION.md](docs/INTEGRATION.md) |
| Testes | `tests/` | [docs/TESTING.md](docs/TESTING.md) |
| Memória de sessão | vault `90 - Agent Workspace/EdgeHub/Client SDK/` | skill pessoal `memory-bank` (não substitui `docs/`) |
| Release | `releases/v0.3.2/` | `SYNC.txt`, `MANIFEST.txt` |
| Firmware EcoCircuito | master `../ecocircuito-maq_v3.0_refatorado`; slave `../ecocircuito-maq_v3.0_slave_refatorado` | slave: submodule `third_party/edgehub-client-sdk` — **não** pôr sob `components/` (CMake host desta lib parte o IDF) |

**Idioma das respostas:** português. Código/comentários: seguir o ficheiro existente.

## Comandos

```powershell
.\scripts\test_host.ps1 -DeviceSdk
# -DEHC_PORT=host|linux|embedded  (esp32 = examples/esp32_demo no IDF)
.\scripts\package_release.ps1 -Version 0.3.3
```

## Armadilhas (gotchas)

Índice curto para o Cursor. Detalhe + payloads: [`docs/`](docs/README.md).

| Data | Área | Lição |
|------|-------|-------|
| 2026-09 | Flags | `memset(&user,0)` = master; usar `disable_*`, não `enable_*=0`. |
| 2026-09 | Device ABI | Não desligar os **três** `disable_*` — `eh_client_init` re-liga tudo. Slave só `disable_telemetry=1`. |
| 2026-09 | CTest | Testes device in-tree: prefixo `eh_` (evitar colisão com o client). |
| 2026-09 | ESP32 HTTP | 401 EdgeHub = transporte OK; port sem `esp_http_client_perform`. |
| 2026-09 | ESP32 relógio | `now_ms` no ingest tem de ser epoch Unix. `esp_timer` → `ts` 1970 → `TELEMETRY_INVALID_TS` (HMAC válido). Port: `gettimeofday` + HTTP `Date`. SNTP no firmware. Re-ler `ehc_now()` após check-in antes do poll. |
| 2026-09 | Heap ESP32 | Check-in não pode manter 12 KB alocados durante `credential/issue`. Poll: não pôr `EH_MAX_RESPONSE` na stack da task MDM. |
| 2026-09 | Glue | `config.apply` / NVS legado: firmware, não esta lib. |
| 2026-09 | Import | `device/` por cópia (sem `git subtree` — cria commit). |
| 2026-09 | HMAC | Números JSON = `JSON.stringify`. `%.15g` em `|x|<1e-4` → 401 ingest; check-in no mesmo secret continua 200. |

Mais detalhe de sessão: vault `90 - Agent Workspace/EdgeHub/Client SDK/lessons.md`. Runbooks: [docs/RUNBOOK_HMAC.md](docs/RUNBOOK_HMAC.md) · [docs/RUNBOOK_ESP32.md](docs/RUNBOOK_ESP32.md).

## Mapa de rules

| Rule | Quando | Tema |
|------|--------|------|
| `delivery-workflow.mdc` | **Sempre** | Proposta → aprovação → memória |
| `ehc-ports.mdc` | **Sempre** | Linux vs embarcado — só o port muda |

## Mapa de skills

| Skill | Quando usar |
|-------|-------------|
| `memory-bank` | Início/fim de tarefa não trivial; contexto e histórico de **sessão** |
| `skill-evolution` | Transformar aprendizagens em rule/skill/memória |
| `ehc-porting` | Novo port / MCU / Linux / ESP32 |

## Memória persistente

Vault: `C:\obsidian_vault\90 - Agent Workspace\EdgeHub\Client SDK\`. Contrato HTTP/ABI → `docs/` neste git. **Não** dual-write o contrato para a memória de sessão.

**Ler** no início de: bugs, continuação de sessão, features multi-ficheiro.

**Atualizar** ao concluir: WIP, issues, armadilha nova.

## Agentes (`.cursor/agents/`)

| Agente | Papel |
|--------|---------|
| `feature-proposal` | Proposta técnica — **sem código** |
| `proposal-implementer` | Implementa **só** o aprovado |
| `sdk-review` | Revisão só-leitura (ABI, ports, flags, HTTP) |

Playbook: [docs/cursor-agents-playbook.md](docs/cursor-agents-playbook.md).

## Pós-implementação

Ao fechar tarefa com alteração de código:

- [ ] Escopo mínimo — sem refactors não pedidos
- [ ] `.\scripts\test_host.ps1 -DeviceSdk` se o host CMake / `src/` / `device/` mudou
- [ ] Atualizar vault `90 - Agent Workspace/EdgeHub/Client SDK/` (`active-context`, `progress`)
- [ ] Armadilha nova em `lessons.md` no vault
- [ ] Contrato/port/runbook → `docs/` neste git
- [ ] Skill/rule se o procedimento Cursor for reutilizável
- [ ] **Não** criar commit salvo pedido explícito

## Fluxo de entrega

1. Proposta (`feature-proposal`) → aprovação
2. Plano opcional em `.cursor/plans/*.plan.md`
3. Implementação (`proposal-implementer` ou agente principal)
4. Revisão (`sdk-review`) se ABI/ports/flags
5. Testes host quando aplicável
