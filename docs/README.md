# Docs — EdgeHub Client SDK

Contrato **público** da biblioteca (umbrella **0.3.3**, device **1.2.6**, ABI **2**). Quem clona este repositório **não precisa** do Obsidian: estes ficheiros são a fonte para integrar, portar e debugar.

Runtime Cursor (só quem trabalha neste git com agentes): `AGENTS.md` e `.cursor/`. Memória de sessão pessoal fica fora deste repositório. Changelog: `CHANGELOG.md`.

| Ficheiro | Conteúdo |
|----------|----------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Camadas, ABI, prontidão, storage, RAM compile-time |
| [API.md](API.md) | `ehc_*`: códigos, `ehc_user_t`, tick, telemetria, comandos |
| [PROTOCOL.md](PROTOCOL.md) | Rotas HTTP, payloads, erros, handshake, downlink |
| [HMAC.md](HMAC.md) | Canónico vs body, números `JSON.stringify`, goldens |
| [INTEGRATION.md](INTEGRATION.md) | Firmware: CMake, identidade, loop, `disable_*`, slave |
| [PORTING.md](PORTING.md) | HAL `ehc_port_t`, Linux, ESP32, MCU, NVS legado |
| [TESTING.md](TESTING.md) | Unity/CTest host |
| [RUNBOOK_HMAC.md](RUNBOOK_HMAC.md) | 401 ingest/check-in |
| [RUNBOOK_ESP32.md](RUNBOOK_ESP32.md) | Relógio, HTTP `perform`, heap, NVS |
| [cursor-agents-playbook.md](cursor-agents-playbook.md) | Prompts Cursor (opcional) |

Device in-tree: `device/docs/INTEGRATION.md` (API `eh_*`). Preferir a camada `ehc_*` deste umbrella.

Não colar HMAC/PKCS#8 de equipamentos de campo nem `.env` nestes ficheiros. Placeholders de teste (`test-secret`, `demo-group`) **não** são produção.
