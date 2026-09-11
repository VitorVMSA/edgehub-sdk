# Estrutura de colaboração com IA — EdgeHub Client SDK

O conhecimento do projeto vive em Git: instruções, padrões, memória entre sessões, agentes e docs. Entrada única para agentes: [`AGENTS.md`](AGENTS.md).

---

## Visão geral

```
edgehub-client-sdk/
├── AGENTS.md
├── README.md
├── AI-WORKFLOW.md
│
├── .cursor/
│   ├── agents/                # papéis (propor / implementar / rever)
│   ├── rules/                 # restrições (.mdc)
│   ├── skills/                # how-to (SKILL.md)
│   └── plans/                 # planos multi-etapa
│
├── memory-bank/               # memória persistente entre sessões
│   ├── active-context.md      # foco + decisões + histórico recente
│   ├── progress.md            # o que funciona / issues
│   ├── lessons-learned.md     # causa-raiz + fix
│   └── system-patterns.md     # arquitetura resumida
│
├── docs/                      # docs canónicas (índice: docs/README.md)
├── device/                    # protocolo ABI 2 (1.2.4)
├── include/  src/             # camada produto
├── port/                      # HAL por alvo
└── releases/v0.3.0/
```

---

## Blocos

| Pasta/ficheiro | Para quê |
|----------------|----------|
| `AGENTS.md` | Índice + gotchas + comandos |
| `.cursor/agents/*.md` | Propor / implementar / rever |
| `.cursor/rules/*.mdc` | O que respeitar |
| `.cursor/skills/*/SKILL.md` | Como fazer |
| `.cursor/plans/*.plan.md` | Planos com todos |
| `memory-bank/*.md` | Contexto recente e histórico entre chats |
| `docs/` | Contrato, ports, testes |

---

## Memory bank — contexto e histórico

Ler no início de trabalho não trivial. Escrever ao fechar feature. Skill: `.cursor/skills/memory-bank/`.

| Ficheiro | Conteúdo |
|----------|----------|
| `active-context.md` | Foco, WIP, decisões, **histórico recente da sessão** |
| `progress.md` | O que já funciona, issues, versões |
| `lessons-learned.md` | Armadilhas com causa, fix e o que evitar |
| `system-patterns.md` | Layout, ports, consumo firmware |

Não duplicar `AGENTS.md`. Memória = estado dinâmico; AGENTS = regras permanentes.

---

## Fluxo

1. Pedido novo → `feature-proposal` (sem código).
2. Aprovação explícita no chat → `proposal-implementer`.
3. Mudanças de ABI, ports ou flags → `sdk-review` (só leitura) se o risco o justificar.
4. Ao fechar → `memory-bank/`, docs canónicas, skills/rules se a aprendizagem for reutilizável.

Prompts: [`docs/cursor-agents-playbook.md`](docs/cursor-agents-playbook.md).

---

## Rules vs skills

- **Rule** — o que respeitar (ports, fluxo de entrega).
- **Skill** — como fazer (portar MCU, atualizar memória, evoluir skills).

---

## Template genérico

A estrutura segue o template em `C:\workspace\AI-WORKFLOW.md`. Neste repo o conteúdo é o da lib C99 (não Docusaurus, Prisma ou firmware ESP32).
