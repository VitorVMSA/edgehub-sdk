---
name: memory-bank
description: >-
  Como ler e atualizar a memória persistente entre sessões (memory-bank/).
  Usar no início de tarefas multi-ficheiro, continuação de trabalho anterior,
  ou ao fechar feature/correção relevante.
---

# Memory Bank — EdgeHub Client SDK

## Ficheiros

| Ficheiro | Conteúdo | Atualizar quando |
|----------|----------|------------------|
| `memory-bank/active-context.md` | Foco, WIP, decisões, histórico recente da sessão | Mudança de foco ou entrega parcial |
| `memory-bank/progress.md` | O que funciona, issues, linha de versões | Testes host ou fecho de feature |
| `memory-bank/lessons-learned.md` | Causa-raiz + fix | Bug não trivial, nova armadilha |
| `memory-bank/system-patterns.md` | Arquitetura resumida | Mudança de layout/ports/ABI |

## Quando LER (obrigatório)

- Início de tarefa multi-ficheiro ou bug complexo
- Continuação de conversa/plano anterior
- Antes de propor arquitetura (ports, device in-tree, flags, firmware)

## Quando ESCREVER

- Feature ou correção concluída
- Descoberta de armadilha (CTest names, HTTP 401, `disable_*`, ABI)
- Decisão de arquitetura que afeta vários caminhos

## Formato `lessons-learned.md`

```markdown
### YYYY-MM | Área | Título curto
**Causa:** ...
**Fix:** ...
**Evitar:** ...
```

## Regras

- Não duplicar `AGENTS.md` — memória é estado dinâmico e histórico de sessão.
- Entradas curtas e acionáveis.
- Manter `active-context.md` enxuto (últimas 2–4 semanas de foco).
- Fluxo completo: `AI-WORKFLOW.md`.

## Validação

Após atualizar: `active-context` reflete o WIP; `progress` lista issues reais, não hipóteses.
