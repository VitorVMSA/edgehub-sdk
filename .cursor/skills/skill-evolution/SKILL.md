---
name: skill-evolution
description: >-
  Transforma aprendizagens de sessão em novas skills, rules ou entradas no
  memory-bank. Usar após bugs resolvidos, padrões novos ou procedimentos
  repetidos que merecem documentação versionada.
---

# Evolução de skills e rules

## Quando usar

- Mesmo procedimento pedido 2+ vezes (port novo, release, testes host)
- Bug com causa-raiz não óbvia (CTest, HTTP 401, ABI)
- Nova convenção acordada com o utilizador

## Decisão: rule vs skill vs memória

| Tipo | Usar para |
|------|-----------|
| **Rule** (`.cursor/rules/*.mdc`) | Restrição permanente — "sempre respeitar" |
| **Skill** (`.cursor/skills/*/SKILL.md`) | Procedimento passo-a-passo — "como fazer" |
| **lessons-learned** | Incidente pontual com causa e fix |
| **AGENTS.md gotcha** | Erro recorrente que todo agente deve ver de imediato |

## Passos

1. Identificar aprendizagem concreta (não genérica).
2. Verificar se já existe em `AGENTS.md`, rules ou `lessons-learned` — evitar duplicação.
3. Criar ou atualizar o artefacto mais adequado (curto e acionável).
4. Referência cruzada em `AGENTS.md` se for crítico.
5. Atualizar `docs/README.md` só se novo doc canónico.

## Formato skill nova

```
.cursor/skills/<nome>/SKILL.md
---
name: nome
description: O que faz e QUANDO usar
---
## Quando usar
## Passos
## Armadilhas
## Validação
```

## Formato rule nova

```
.cursor/rules/<nome>.mdc
---
description: ...
globs: ...
alwaysApply: false
---
# Título
- bullets acionáveis
```

## Validação

- Rule/skill curtas; detalhe longo em `docs/`
- Não commitar sem pedido

## Exemplos neste SDK (2026-09)

| Aprendizagem | Artefacto |
|--------------|-----------|
| CTest `test_checkin` colide | `lessons-learned` + prefixo `eh_` |
| `enable_*` vs memset 0 | gotcha AGENTS + `docs/INTEGRATION.md` |
| Três `enable_*=0` no device | `docs/INTEGRATION.md` + lessons |
| HTTP 401 ESP32 | `ehc-ports.mdc` + `docs/PORTING.md` |
| ESP32 `now_ms` 1970 / `TELEMETRY_INVALID_TS` | gotcha AGENTS + `docs/PORTING.md` + lessons |
| Check-in+issue heap WROOM | `lessons-learned` + `docs/PORTING.md` |
| Só o port muda | rule `ehc-ports` + skill `ehc-porting` |
