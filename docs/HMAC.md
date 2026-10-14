# HMAC e números JSON

HTTP: [PROTOCOL.md](PROTOCOL.md). Sintomas: [RUNBOOK_HMAC.md](RUNBOOK_HMAC.md).

## Algoritmo

`eh_hmac_sha256_hex(secret, canonical_utf8, out_hex[65])`:

- HMAC-SHA256 RFC 2104.
- Se `strlen(secret) > 64`, a chave é `SHA256(secret)` (bloco 64).
- Digest em **hex minúsculo** 64 chars + NUL.
- Header: `x-edgehub-device-signature: <hex>`.

Secret: string UTF-8 do `credential.secret` (não é a group key). Persistência `ehc.hmac.secret` / `.prev`.

## Canónico ≠ body (armadilha nº 1)

| Operação | Body enviado | String HMAC |
|----------|--------------|-------------|
| Check-in | `{deviceUid, groupKey?, productCode?, keys}` | **o body** (sem `alias`) |
| Telemetria legado | `{deviceUid, ts, values}` **sem** alias | `{deviceUid, alias:"", ts, values}` |
| Telemetria lote | `{deviceUid, readings:[...]}` **sem** alias | `{deviceUid, alias:"", readings:[...]}` |
| Poll | `{deviceUid, ts, values:{}}` | `{deviceUid, alias:"", ts, values:{}}` |
| executed | `{deviceUid, message?, success?}` | `{deviceUid, alias:"", executionId, message?, success?}` |
| credential/issue | `{deviceUid, challengeId, signature}` | Ed25519 sobre `{challengeId, deviceUid, nonce}` — **não** HMAC |

O servidor HMAC-a `JSON.stringify` do objecto **parseado**. Espaços: a lib emite **sem espaços**. Não assinar o raw body de telemetria.

`alias` é campo da plataforma; o firmware **nunca** o envia no body. O canónico força `""`.

Check-in: HMAC usa tipos originais das `keys`. Persistência servidor `lastSeenKeysJson` normaliza strings — não stringificar números só para HMAC.

## Números = `JSON.stringify` (ECMA-262 NumberToString)

Implementação: `eh_json_print_number` em `device/src/internal/eh_json.c` (desde device **1.2.5** / umbrella **0.3.1**). Usado por `eh_values_add_num`. **Não** imprimir métricas à mão com `%.15g` / `%.17g`.

Algoritmo:

1. NaN ou Inf → JSON `null`.
2. `0` → `0`.
3. Shortest round-trip: tenta precisão 1..17 com `%.*g` no absoluto; aceita o primeiro que `strtod` recupera **bit-exact**.
4. Reformat ES: `es_format_abs` com regras `-6 < n ≤ 21` (decimal) senão científico `e` com expoente **sem zeros à esquerda** (`e-7` não `e-007`).

| Valor | `%.15g` (errado) | JSON.stringify / lib |
|------:|------------------|----------------------|
| `1e-5` | `1e-05` ou `e-005` | `0.00001` |
| `1e-4` | `0.0001` | `0.0001` |
| `1e21` | científico ou 21 dígitos | regras ES |
| `22.5` | `22.5` | `22.5` |

`%.17g` sozinho gera **dígitos a mais** que também falham o HMAC.

Sintoma: lote com peso/energia `|x|<1e-4` → 401 `TELEMETRY_DEVICE_CREDENTIAL_INVALID` **no mesmo secret** que faz check-in 200 (keys são strings). Testes: `eh_test_values_writer`, `eh_test_hmac_golden` com `1e-5`.

**Não** «corrigir» no glue do produto nem no servidor. O printer vive no device.

## Retry `secret_previous`

Servidor aceita o secret antigo até `rotateGraceUntil`. A lib:

1. POST com `secret_current`.
2. Se HTTP **401** e body contém token `DEVICE_CREDENTIAL_INVALID` e há previous → log `hmac_retry_previous` → **um** segundo POST com previous.
3. **Não** retry em EXPIRED / REVOKED / REQUIRED / SIGNATURE.

Harness/host stub **têm** de consumir a reply matching no 1.º POST; senão o 2.º POST reutiliza o mesmo 401 e o teste mente.

Após issue, `set_secret(new, old_current)` para a janela de graça.

## `expiresAt`

ISO-8601 parseado para `credential_expires_at_ms`. No `eh_client_tick`, se `now_ms >= expires` → `needs_credential_reprovision`. Comparação inútil com relógio de uptime — a recuperação nesse hardware é o 401 EXPIRED. Port ESP32: `gettimeofday`.

## Testes golden (`device/tests/test_hmac_golden.c`)

Harness: `device_uid=dev-1`, `base_url=https://example.edgehub.test`, collect default `"temp":22.5` se `collect_num==0`. Secret `test-secret` / `prev-secret` (**não** são de produção).

| Caso | Canónico HMAC (sem espaços) | Header hex |
|------|-----------------------------|------------|
| Single `temp=22.5`, `ts=2026-01-01T00:00:00.000Z` | `{"deviceUid":"dev-1","alias":"","ts":"2026-01-01T00:00:00.000Z","values":{"temp":22.5}}` | `4c61fc86abc624e18c8b96ca2a3c37a4ea7291099b76e96ed349cf4c2f82e0b6` |
| Lote 2 leituras (`22.5` + `23`, 1 s depois) | `{"deviceUid":"dev-1","alias":"","readings":[{"ts":"2026-01-01T00:00:00.000Z","values":{"temp":22.5}},{"ts":"2026-01-01T00:00:01.000Z","values":{"temp":23}}]}` | `74f4517a89861f171d1bf9a135266cf82346994f23c8785bba02af6f4a73980f` |
| `temp=1e-5` (stringify `0.00001`) | idem single com `"temp":0.00001` | `c2c3a79fd8fe0de566bbb406c99374a2a3ce30ac201c15d8e67b71f30f256bce` |
| executed sucesso `exec-1` (omite `success`/`message`) | `{"deviceUid":"dev-1","alias":"","executionId":"exec-1"}` | `991fdc5a9f59ee150ffc830843e67b096b1b9404e03bbbca3f57b3200be47130` |
| executed falha `exec-1` + `"nope"` | `{"deviceUid":"dev-1","alias":"","executionId":"exec-1","message":"nope","success":false}` | `a0e3bb6f62412573d5364fc90a98a0a8c18a1671608edd32525b2b93ef8e6529` |
| Retry previous: 2.º POST com `prev-secret` (mesmo canónico single 22.5) | (igual ao single) | `0b8ee605ade28678e569d1caba1826b997d4bb86d1192a2285dc7f3927c1ede0` |

Body de telemetria **não** contém `"alias"`. O 1.º POST do retry usa o hex do single (`4c61fc86…`) com `test-secret`.

Host stub injeta no retry:

```json
{"code":"TELEMETRY_DEVICE_CREDENTIAL_INVALID"}
```

Regenerar estes hex no Node: `createHmac('sha256', secret).update(canonical, 'utf8').digest('hex')`. Divergência byte-a-byte = bug de printer, não de secret.
