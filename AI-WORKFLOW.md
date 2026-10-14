# Estrutura de colaboração com IA — EdgeHub Client SDK

**Contrato da biblioteca:** [`docs/README.md`](docs/README.md).
**Este git também tem:** runtime Cursor (`AGENTS.md`, `.cursor/`) + código. Entrada: [`AGENTS.md`](AGENTS.md).

Contrato HTTP/payloads/ports vive em `docs/`. Memória de sessão fica no vault pessoal e não faz parte deste repositório.

---

## Visão geral

```
edgehub-client-sdk/
├── AGENTS.md
├── README.md
├── AI-WORKFLOW.md
│
├── .cursor/
│   ├── agents/
│   ├── rules/
│   ├── skills/
│   └── plans/
│
├── docs/                      # contrato partilhável
├── device/                    # protocolo ABI 2 (1.2.6)
├── include/  src/
├── port/
└── releases/v0.3.2/
```

---

## Blocos

| Pasta/ficheiro | Para quê |
|----------------|----------|
| `AGENTS.md` | Índice Cursor + gotchas + comandos |
| `.cursor/agents/*.md` | Propor / implementar / rever |
| `.cursor/rules/*.mdc` | O que respeitar |
| `.cursor/skills/*/SKILL.md` | Como fazer |
| `.cursor/plans/*.plan.md` | Planos com todos |
| `docs/` | Contrato HTTP, ports, testes, runbooks (partilhável) |

---

## Memory bank — contexto e histórico

Ler no início de trabalho não trivial e atualizar ao fechar feature. Skill: `.cursor/skills/memory-bank/`; os ficheiros de sessão ficam no vault pessoal fora deste git.

Não duplicar `AGENTS.md` nem `docs/`. Memória = estado dinâmico; `docs/` = contrato; AGENTS = regras permanentes do Cursor.

---

## Fluxo

1. Pedido novo → `feature-proposal` (sem código).
2. Aprovação explícita no chat → `proposal-implementer`.
3. Mudanças de ABI, ports ou flags → `sdk-review` (só leitura) se o risco o justificar.
4. Ao fechar → memória de sessão no vault + `docs/` se o contrato mudou.

Prompts: [`docs/cursor-agents-playbook.md`](docs/cursor-agents-playbook.md).

---

## Rules vs skills

- **Rule** — o que respeitar (ports, fluxo de entrega).
- **Skill** — como fazer (portar MCU, atualizar memória, evoluir skills).

A convenção do vault EcoCircuito (se existir) não substitui `docs/` deste repo. **Não** apagar `docs/` para «ficar só no Obsidian» — esta biblioteca é partilhável.
