---
name: memory-bank
description: >-
  Como ler e atualizar a memória persistente de sessão no vault pessoal do EdgeHub Client SDK.
  Usar no início de tarefas multi-ficheiro, continuação de trabalho anterior,
  ou ao fechar feature/correção relevante.
---

# Memory Bank — EdgeHub Client SDK

## Ficheiros

| Ficheiro | Conteúdo | Atualizar quando |
|----------|----------|------------------|
| `90 - Agent Workspace/EdgeHub/Client SDK/active-context.md` | Foco, WIP, decisões, histórico recente da sessão | Mudança de foco ou entrega parcial |
| `90 - Agent Workspace/EdgeHub/Client SDK/progress.md` | O que funciona, issues, linha de versões | Testes host ou fecho de feature |
| `90 - Agent Workspace/EdgeHub/Client SDK/lessons.md` | Causa-raiz + fix | Bug não trivial, nova armadilha |
| `90 - Agent Workspace/EdgeHub/Client SDK/system-patterns.md` | Arquitetura resumida | Mudança de layout/ports/ABI |

## Quando LER (obrigatório)

- Início de tarefa multi-ficheiro ou bug complexo
- Continuação de conversa/plano anterior
- Antes de propor arquitetura (ports, device in-tree, flags, firmware)

## Quando ESCREVER

- Feature ou correção concluída
- Descoberta de armadilha (CTest names, HTTP 401, `disable_*`, ABI)
- Decisão de arquitetura que afeta vários caminhos

## Formato `lessons.md`

```markdown
### YYYY-MM | Área | Título curto
**Causa:** ...
**Fix:** ...
**Evitar:** ...
```

## Regras

- Não duplicar `AGENTS.md` — memória é estado dinâmico e histórico de sessão.
- Não duplicar `docs/` (contrato da biblioteca) na memória de sessão.
- Entradas curtas e acionáveis.
- Manter `active-context.md` enxuto (últimas 2–4 semanas de foco).
- Fluxo completo: `AI-WORKFLOW.md`.

## Validação

Após atualizar: `active-context` reflete o WIP; `progress` lista issues reais, não hipóteses.
