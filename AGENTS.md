# AGENTS.md — EdgeHub Client SDK

Entrada única para agentes. Leia este ficheiro antes de propor ou implementar. Estrutura de colaboração: [`AI-WORKFLOW.md`](AI-WORKFLOW.md).

Umbrella **0.3.1**: protocolo in-tree (`device/` = edgehub-device-sdk **1.2.5**, ABI 2) + camada de produto.

| Área | Caminho | Doc |
|------|---------|-----|
| Versão | `VERSION`, `EHC_LIB_VERSION` em `include/edgehub_client/ehc.h` | `CHANGELOG.md` |
| Device | `device/` (1.2.4, ABI 2) | `device/README.md` se existir |
| Client | `include/edgehub_client/`, `src/` | `docs/INTEGRATION.md` |
| Ports | `port/{host_stub,linux,esp32,embedded}/` | `docs/PORTING.md` |
| Template produto | `user_template/` | `docs/INTEGRATION.md` |
| Testes | `tests/` | `docs/TESTING.md` |
| Docs | `docs/` | `docs/README.md` |
| Memória | `memory-bank/` | skill `memory-bank` |
| Release | `releases/v0.3.1/` | `SYNC.txt`, `MANIFEST.txt` |
| Firmware EcoCircuito | `../ecocircuito-maq_v3.0_refatorado` | um submodule `components/edgehub_sdk` — **não** migrar nesta entrega |

**Idioma das respostas:** português. Código/comentários: seguir o ficheiro existente.

## Comandos

```powershell
.\scripts\test_host.ps1 -DeviceSdk
# -DEHC_PORT=host|linux|embedded  (esp32 = examples/esp32_demo no IDF)
.\scripts\package_release.ps1 -Version 0.3.1
```

## Armadilhas (gotchas)

| Data | Área | Lição |
|------|-------|-------|
| 2026-09 | Flags | `memset(&user,0)` = master; usar `disable_*`, não `enable_*=0`. |
| 2026-09 | Device ABI | Não desligar os **três** `disable_*` — `eh_client_init` re-liga tudo. Slave só `disable_telemetry=1`. |
| 2026-09 | CTest | Testes device in-tree: prefixo `eh_` (evitar colisão com o client). |
| 2026-09 | ESP32 HTTP | 401 EdgeHub = transporte OK; port sem `esp_http_client_perform`. |
| 2026-09 | Glue | `config.apply` / NVS legado: firmware, não esta lib. |
| 2026-09 | Import | `device/` por cópia (sem `git subtree` — cria commit). |
| 2026-09 | HMAC | Números JSON = `JSON.stringify`. `%.15g` em `|x|<1e-4` → 401 ingest; check-in no mesmo secret continua 200. |

Mais detalhe: `memory-bank/lessons-learned.md`.

## Mapa de rules

| Rule | Quando | Tema |
|------|--------|------|
| `delivery-workflow.mdc` | **Sempre** | Proposta → aprovação → memória |
| `ehc-ports.mdc` | **Sempre** | Linux vs embarcado — só o port muda |

## Mapa de skills

| Skill | Quando usar |
|-------|-------------|
| `memory-bank` | Início/fim de tarefa não trivial; contexto e histórico |
| `skill-evolution` | Transformar aprendizagens em rule/skill/memória |
| `ehc-porting` | Novo port / MCU / Linux / ESP32 |

## Memória persistente (`memory-bank/`)

**Ler** no início de: bugs, continuação de sessão, features multi-ficheiro, decisões de arquitetura.

**Atualizar** ao concluir: feature, correção, armadilha nova, mudança de ABI/port.

| Ficheiro | Conteúdo |
|----------|----------|
| `active-context.md` | Foco, WIP, decisões, histórico recente |
| `progress.md` | O que funciona / issues |
| `lessons-learned.md` | Causa-raiz + fix |
| `system-patterns.md` | Arquitetura + links |

## Agentes (`.cursor/agents/`)

| Agente | Papel |
|--------|---------|
| `feature-proposal` | Proposta técnica — **sem código** |
| `proposal-implementer` | Implementa **só** o aprovado |
| `sdk-review` | Revisão só-leitura (ABI, ports, flags, HTTP) |

Playbook: `docs/cursor-agents-playbook.md`.

## Pós-implementação

Ao fechar tarefa com alteração de código:

- [ ] Escopo mínimo — sem refactors não pedidos
- [ ] `.\scripts\test_host.ps1 -DeviceSdk` se o host CMake / `src/` / `device/` mudou
- [ ] Atualizar `memory-bank/active-context.md` e `progress.md`
- [ ] Armadilha nova em `memory-bank/lessons-learned.md`
- [ ] Doc canónica só se o contrato/port mudou
- [ ] Skill/rule se o procedimento for reutilizável
- [ ] **Não** criar commit salvo pedido explícito

## Fluxo de entrega

1. Proposta (`feature-proposal`) → aprovação
2. Plano opcional em `.cursor/plans/*.plan.md`
3. Implementação (`proposal-implementer` ou agente principal)
4. Revisão (`sdk-review`) se ABI/ports/flags
5. Testes host quando aplicável

## Links rápidos

- Índice de docs: `docs/README.md`
- Estrutura IA: `AI-WORKFLOW.md`
- README técnico: `README.md`
