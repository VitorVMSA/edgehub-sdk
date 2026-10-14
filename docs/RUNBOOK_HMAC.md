# Runbook HMAC 401

Teoria: [HMAC.md](HMAC.md). HTTP: [PROTOCOL.md](PROTOCOL.md).

## Árvore de decisão

1. **Check-in 200 + ingest 401 `TELEMETRY_DEVICE_CREDENTIAL_INVALID` no mesmo secret**
   → stringify de números (`|x|<1e-4` → `1e-05` vs `0.00001`). Device tem de ser ≥ 1.2.5. **Não** re-issue PKCS#8. **Não** «rodar» o HMAC no painel.

2. **Check-in e ingest 401 INVALID**
   → secret errado / truncado / NVS mapeada mal. Ver storage legado em [PORTING.md](PORTING.md). Retry previous só cobre a janela `rotateGraceUntil`.

3. **401 EXPIRED / REVOKED / REQUIRED**
   → reprovision: próximo check-in **sem** HMAC; esperar `identityChallenge`; issue. **Não** retry previous.

4. **401 `TELEMETRY_INVALID_TS`**
   → validação de timestamp; confira relógio e ISO-8601. O código não prova por si só que a assinatura está correcta. [RUNBOOK_ESP32.md](RUNBOOK_ESP32.md). Pode vir `rejectedIndex`.

5. **401 `CHECKIN_DEVICE_IDENTITY_NOT_REGISTERED`**
   → PKCS#8/UID não no identity-api. Backoff 300 s se sem `Retry-After`.

6. **Check-in 401 sem challenge**
   → log `checkin_401_without_identity_challenge`. Issue não corre. Corpo 401 tem de chegar ao SDK (port sem `esp_http_client_perform`).

## O que NÃO fazer

- Patch só no glue do produto ou alterar o contrato do servidor para aceitar `1e-05`.
- Usar `%.15g` / `%.17g` no body canónico.
- Assinar o raw body de telemetria (falta `alias:""`).
- Reutilizar a mesma reply HTTP nos testes de retry previous.
- Tratar 401 de aplicação como RST.
- Re-issue em ciclo num equipamento já `active` (401 ingest + check-in 200 = stringify).
