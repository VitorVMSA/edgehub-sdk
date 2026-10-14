# Protocolo HTTP do device

Implementação C em `device/` (1.2.6). HMAC: [HMAC.md](HMAC.md). Integração `ehc_*`: [INTEGRATION.md](INTEGRATION.md). API: [API.md](API.md).

`base` = `eh_config_t.base_url` **sem** path `/api/v1/...` (senão o path duplica → 404).
`tenant` = `tenant_slug`. Se vazio, a lib omite o segmento (muitos tenants **exigem** o slug — 404 típico sem ele).

Todas as rotas desta lib são **POST**. Não há `http_get` no HAL. O GET de `downloadUrl` (OTA) é da aplicação em `on_download`.

Auth comum após issue: header `x-edgehub-device-signature: <64 hex HMAC-SHA256>`. Sempre `Content-Type: application/json`. Sem `WWW-Authenticate` nos 401 de aplicação — o port **tem** de devolver o body em 4xx.

Sucesso de transporte: `http_post` / `http_post_ex` **nonzero** mesmo em 4xx/5xx, com `http_status` preenchido. `Retry-After` (segundos) em qualquer status → `last_retry_after_sec` → backoff.

---

## 1. Check-in MDM

| | |
|---|---|
| Método | `POST` |
| Path com tenant | `{base}/api/v1/mdm/public/{tenant}/device/checkin` |
| Path sem tenant | `{base}/api/v1/mdm/public/device/checkin` |
| Auth | HMAC do **body** se existe `secret_current` **e** `needs_credential_reprovision=0`. Caso contrário só `Content-Type` |
| Canónico HMAC | **o próprio body** (não insere `alias`) |
| Query | nenhum |

### Body mínimo (sem `groupKey` nem `productCode`)

Só `deviceUid` + `keys`. Servidor: `pending`, sem produto. Rejeitado (`400 CHECKIN_FULL_REQUIRED`) se o device já tem catálogo ou está `active`.

```json
{"deviceUid":"SERIAL123","keys":{"firmware":"0.1.0","boardModel":"example_board"}}
```

| Campo | Tipo | Obrigatório | Notas |
|-------|------|-------------|-------|
| `deviceUid` | string | sim | JSON-escaped; firmware **não** envia `alias` |
| `keys` | object string→string | sim (pode ser `{}`) | versões de asset / firmware / board |

### Body completo

`groupKey` **e** `productCode` juntos. Um só → servidor `CHECKIN_MODE_INVALID`. `groupKey` é segredo de org — não commitar valores de campo.

```json
{"deviceUid":"SERIAL123","groupKey":"<org-secret>","productCode":"yourProduct","keys":{"firmware":"1.1.57","boardModel":"board_x","classifications":"3"}}
```

| Campo | Tipo | Obrigatório no modo completo |
|-------|------|------------------------------|
| `groupKey` | string | sim (com product) |
| `productCode` | string | sim (com group) |
| `keys.<assetKey>` | string | se a key está na tabela; `EHC_KEY_ASSET` omite-se se não há versão persistida |

`keys` vêm de `checkin_keys_collect` / tabela `EHC_KEY_*`. Tecto `EH_MAX_CHECKIN_KEYS` = 768 bytes JSON.

### Resposta 200

A lib lê:

| Campo | Tipo | Obrigatório para a lib | Notas |
|-------|------|------------------------|-------|
| `provisioningStatus` | `"pending"` \| `"active"` | não | mapeia `EH_PROV_*` |
| `identityChallenge` | object | não | dispara issue se PKCS#8/flag |
| `identityChallenge.challengeId` | string | se challenge | |
| `identityChallenge.nonce` | string | se challenge | **sem** `expiresAt` no CI feliz |
| `downloads` | map assetKey → object | não | só se `on_checkin_download` |
| `downloads.*.version` | string | sim para oferecer | skip se ausente |
| `downloads.*.downloadUrl` | string | sim para oferecer | |
| `downloads.*.checksumSha256` | string hex | não | vazio se ausente |
| `configurations` | map assetKey → object | não | |
| `configurations.*.version` | string | sim para oferecer | |
| `configurations.*.source` | string | não | `""` se ausente |
| `configurations.*.payload` | object JSON | sim | raw, até 4096 B |

A lib **não** lê `pendingCommands` no check-in (downlink só no ingest). Recovery `active` sem HMAC: HTTP 200 + challenge + header servidor `x-edgehub-credential-recovery: 1`; o servidor omite downloads/configs/comandos. **A lib C não parseia esse header** — só o body (`identityChallenge` + `provisioningStatus`). Recuperação no firmware = check-in sem HMAC (`needs_credential_reprovision`) + issue, e um retry HMAC com `secret_previous` em 401 `*CREDENTIAL_INVALID`.

### identityChallenge em 4xx

Se o body 4xx ainda traz `identityChallenge`, a lib faz stash num wrap 512 B, **liberta** os buffers de 12 KB, e **depois** `credential/issue`. Se o issue emitir HMAC e não havia secret, o check-in trata-se como sucesso (`rc = EH_OK`).

### Erros check-in que a lib interpreta

| HTTP | `code` no JSON | Efeito |
|------|----------------|--------|
| 401 | `CHECKIN_DEVICE_IDENTITY_NOT_REGISTERED` | backoff **300 s** se o port não enviou `Retry-After` |
| 401 | `CHECKIN_DEVICE_CREDENTIAL_REVOKED` | `needs_credential_reprovision=1` (próximo check-in **sem** HMAC) |
| 401 | `CHECKIN_DEVICE_CREDENTIAL_REQUIRED` | idem |
| 401 | `CHECKIN_DEVICE_CREDENTIAL_EXPIRED` | idem |
| 401 | `CHECKIN_DEVICE_CREDENTIAL_INVALID` | retry HMAC com `secret_previous` **uma vez**; se persistir, reprovision |
| 401 | `CHECKIN_DEVICE_SIGNATURE_REQUIRED` | reprovision |
| 401 | (sem challenge) | log `checkin_401_without_identity_challenge` |
| 4xx/5xx | qualquer | `checkin_fail http=N`; backoff exponencial 2 s…60 s |

Servidor (não parseado na lib, mas o firmware vê): `CHECKIN_FULL_REQUIRED`, `CHECKIN_MODE_INVALID`.

Exemplo 401:

```json
{"code":"CHECKIN_DEVICE_IDENTITY_NOT_REGISTERED","error":"Device identity not registered"}
```

---

## 2. credential/issue

Disparado só se `enable_identity_credential_issue=1` **e** o check-in (2xx ou 4xx) trouxe challenge. O client liga a flag sozinho se `identity_pkcs8` resolver.

| | |
|---|---|
| Método | `POST` |
| Path com tenant | `{base}/api/v1/telemetry/public/{tenant}/device/credential/issue` |
| Path sem tenant | `{base}/api/v1/telemetry/public/device/credential/issue` |
| Auth | **Ed25519**, não HMAC. Header só `Content-Type` |
| Heap | canonical 512 B, body 768 B, resp 1536 B — **depois** de `free` dos 12 KB do check-in |

### Canónico assinado (UTF-8, ordem fixa)

```json
{"challengeId":"ch-1","deviceUid":"SERIAL123","nonce":"nn-1"}
```

Assinatura: `eh_ed25519_sign_utf8` → **Base64URL** (sem `+`/`/`) em `signature`. Seed: 32 bytes (`eh_client_set_identity_seed`) **ou** PKCS#8 Base64/Base64URL (`eh_client_set_identity_private_key_pkcs8_b64`). Decode DER; procura OCTET STRING `0x04 0x20` + 32 bytes (`eh_identity.c`). Sem seed: log `identity_seed_missing`, `EH_ERR_AUTH`. PKCS#8 de teste `dGVzdA==` (= ASCII `test`) **não** é chave válida — só liga a flag no `test_features`.

### Body enviado

```json
{"deviceUid":"SERIAL123","challengeId":"ch-1","signature":"<ed25519-b64url>"}
```

O body **não** é o canónico (o canónico inclui `nonce`; o body inclui `signature`).

### Resposta 2xx

```json
{
  "credential": {
    "secret": "<hmac-secret>",
    "rotateGraceUntil": "2026-09-19T00:00:00.000Z",
    "expiresAt": "2027-09-18T00:00:00.000Z"
  }
}
```

| Campo | Tipo | Obrigatório | Efeito na lib |
|-------|------|-------------|---------------|
| `credential.secret` | string | **sim** | `eh_client_set_secret(new, old_current)` |
| `credential.rotateGraceUntil` | string ISO | não | só log `identity_credential_rotate_grace` |
| `credential.expiresAt` | string ISO-8601 `Z` | não | parse para `credential_expires_at_ms`; no tick, se `now_ms >= expires` → reprovision |

ISO aceite: `YYYY-MM-DDTHH:MM:SS.mmmZ` ou sem milissegundos. 0 ms é forçado a 1 (não colidir com «sem expiração»).

Falha transporte: `credential_issue_transport_fail`. HTTP ≠ 2xx: `credential_issue_fail http=N`. Parse sem `credential.secret`: `EH_ERR_PARSE`.

---

## 3. Ingest de telemetria

| | |
|---|---|
| Método | `POST` |
| Path com tenant | `{base}/api/v1/telemetry/public/{tenant}/device/telemetry` |
| Path sem tenant | `{base}/api/v1/telemetry/public/device/telemetry` |
| Auth | HMAC obrigatório (`has_secret_current`). Sem secret → `EH_ERR_AUTH`, **não** POST |
| Sucesso | HTTP **2xx** (servidor async default **202**) |
| Crédito | conta `telemetry_events` (incluindo heartbeat `values:{}`) |

`ts` ISO-8601 UTC gerado de `now_ms` se a aplicação não passou `ts_iso`:

```
YYYY-MM-DDTHH:MM:SS.mmmZ
```

`now_ms` **tem** de ser epoch Unix em produção. Uptime (`esp_timer`) → `ts` 1970 → `401 TELEMETRY_INVALID_TS` com HMAC válido. Ver [RUNBOOK_ESP32.md](RUNBOOK_ESP32.md).

### 3a. Corpo legado (`telemetry_batch_max` ≤ 1)

**Body (sem `alias`):**

```json
{"deviceUid":"SERIAL123","ts":"2026-09-18T16:36:00.000Z","values":{"temperature":22.5,"motor":{"rpm":0,"running":false}}}
```

**Canónico HMAC (insere `alias:""`):**

```json
{"deviceUid":"SERIAL123","alias":"","ts":"2026-09-18T16:36:00.000Z","values":{"temperature":22.5,"motor":{"rpm":0,"running":false}}}
```

Se `ts` vazio no slot, o body omite o campo `ts` mas o canónico ainda usa a string do slot (poll preenche sempre).

`values:{}` é o heartbeat / poll (slave).

### 3b. Lote (`telemetry_batch_max` > 1)

**Body (sem `alias`):**

```json
{
  "deviceUid": "SERIAL123",
  "readings": [
    {"ts":"2026-09-18T16:36:00.000Z","values":{"type":"NEW_WASTE","weight_g":120.5}},
    {"ts":"2026-09-18T16:36:01.000Z","values":{"type":"IDLE"}}
  ]
}
```

**Canónico HMAC** (`JSON.stringify`, sem espaços):

```json
{"deviceUid":"SERIAL123","alias":"","readings":[{"ts":"2026-09-18T16:36:00.000Z","values":{"type":"NEW_WASTE","weight_g":120.5}},{"ts":"2026-09-18T16:36:01.000Z","values":{"type":"IDLE"}}]}
```

Cada item **precisa** de `ts` distinto. Vários sem `ts` colapsam no mesmo `now` no servidor. Lote é exclusivo de `ts`/`values` no topo. Números: [HMAC.md](HMAC.md).

Cap heap: `EH_MAX_BATCH_PAYLOAD` = 65536 para body **e** canónico.

### Resposta 2xx

Além de ACK, a lib parseia `pendingCommands` (secção 5). Extra na mesma resposta é ignorado.

### Erros ingest

| HTTP | `code` | Efeito |
|------|--------|--------|
| 401 | `TELEMETRY_DEVICE_CREDENTIAL_INVALID` | 1 retry com `secret_previous`; se falhar, `needs_credential_reprovision` |
| 401 | `TELEMETRY_DEVICE_CREDENTIAL_EXPIRED` | reprovision (**sem** retry previous) |
| 401 | `TELEMETRY_DEVICE_CREDENTIAL_REVOKED` | reprovision |
| 401 | `TELEMETRY_DEVICE_CREDENTIAL_REQUIRED` | reprovision |
| 401 | `TELEMETRY_DEVICE_SIGNATURE_REQUIRED` | reprovision |
| 401 | `TELEMETRY_INVALID_TS` | categoria de validação; verificar relógio e timestamp. O código não prova isoladamente que o HMAC estava correcto. Pode vir `rejectedIndex` |
| 401/400 | `rejectedIndex`: N | remove da fila RAM a leitura N do lote enviado (0-based); a API client não expõe este índice |

```json
{"code":"TELEMETRY_INVALID_TS","rejectedIndex":0}
```

```json
{"code":"TELEMETRY_DEVICE_CREDENTIAL_INVALID"}
```

Sintoma clássico: check-in 200 no mesmo secret + ingest 401 INVALID + lote com `|x|<1e-4` → stringify, **não** secret. Ver [RUNBOOK_HMAC.md](RUNBOOK_HMAC.md).

Falha HTTP: a fila RAM normalmente **não** é drenada, mas `rejectedIndex` pode remover uma leitura específica. `ehc_tel_flush` → `EHC_EIO`. A API não devolve o índice rejeitado nem um mapa de ACK por `rec_id`; a aplicação não deve limpar a fila ou confirmar registros automaticamente. Uma aplicação com store durável deve definir política de replay/duplicidade compatível com essa limitação. Saturação da fila RAM é falha local de enqueue, não rejeição remota.

---

## 4. Poll downlink (mesmo ingest)

Não é URL nova. `eh_client_poll_downlink` / `ehc_poll_downlink`:

- Body legado `values:{}` + `ts` de `now_ms` **re-lido** imediatamente antes do POST (o check-in do mesmo tick pode ter sincronizado HTTP `Date`).
- Resp heap **2048** B (não `EH_MAX_RESPONSE` 8192 na stack).
- Log: `downlink_poll ts=...` / `downlink_poll_ok` / `downlink_poll_fail http=N` / `downlink_poll_oom`.
- Sem secret → `EH_ERR_AUTH`.
- Conta crédito `telemetry_events`.

Slave: `disable_telemetry=1` + `poll_commands=1` a cada `EHC_COMMAND_POLL_INTERVAL_MS` (60 s). **Não** faz `schedule_backoff` extra no poll (atrasaria o check-in).

Master com telemetria real **não** precisa de poll — comandos vêm no POST de ingest.

---

## 5. Downlink `pendingCommands` e `executed`

Cap `EH_MAX_COMMANDS` = 5 = `TELEMETRY_DOWNLINK_MAX_COMMANDS`. O 6.º na mesma resposta: log `pending_commands_truncated`, ignorado. Contrato v2: `executionId`, `commandKey`, `payload` (sem `payloadType`). Extra ignorado.

### Array na resposta 2xx do ingest

```json
{
  "pendingCommands": [
    {
      "executionId": "8f3c1a2e-4b90-4c1d-9e2a-11aa22bb33cc",
      "commandKey": "device.reboot",
      "payload": {}
    },
    {
      "executionId": "…",
      "commandKey": "motor.set_speed",
      "payload": {"rpm": 1500}
    }
  ]
}
```

| Campo | Tipo | Se ausente |
|-------|------|------------|
| `executionId` | string | **skip** do objecto |
| `commandKey` | string | `""` se ausente/`null` — **ainda despacha** (`pending_cmd=?`) |
| `payload` | JSON raw | `"{}"` |

`device.force_checkin`: built-in **antes** de `on_command`. Payload `{}`. Não declarar na tabela user (`ehc_init` → `EHC_ECONFIG`).

### POST executed

| | |
|---|---|
| Método | `POST` |
| Path com tenant | `{base}/api/v1/telemetry/public/{tenant}/device/command-executions/{executionId}/executed` |
| Path sem tenant | `{base}/api/v1/telemetry/public/device/command-executions/{executionId}/executed` |
| Auth | HMAC |
| Idempotência servidor | já executed → **200**; ainda `queued` → **409** |

**Body** (sucesso, sem mensagem):

```json
{"deviceUid":"SERIAL123"}
```

**Body** (falha e/ou mensagem):

```json
{"deviceUid":"SERIAL123","message":"rpm fora do intervalo","success":false}
```

`success:true` **omite-se**. `message` omite-se se vazio.

**Canónico HMAC:**

```json
{"deviceUid":"SERIAL123","alias":"","executionId":"8f3c1a2e-4b90-4c1d-9e2a-11aa22bb33cc"}
```

Com mensagem e falha:

```json
{"deviceUid":"SERIAL123","alias":"","executionId":"…","message":"rpm fora do intervalo","success":false}
```

O client liga `enable_auto_confirm_commands=1`: o POST corre **antes** do handler se `EHC_CMD_DEFER_AFTER_ACK` (reboot/OTA). Sem defer, o handler corre primeiro e o `message` vai no executed.

---

## Sequência (handshake + loop)

```mermaid
sequenceDiagram
  participant App
  participant Ehc as ehc_tick
  participant Dev as device/
  participant MDM as mdm public
  participant Tel as telemetry public

  App->>Ehc: ehc_init (PKCS8, group, product)
  App->>Ehc: ehc_tick
  Ehc->>Dev: eh_client_init
  Dev->>MDM: POST checkin (completo, sem HMAC)
  MDM-->>Dev: 200 pending + identityChallenge
  Note over Dev: free 12KB; malloc issue pequeno
  Dev->>Tel: POST credential/issue (Ed25519)
  Tel-->>Dev: 200 credential.secret
  Dev-->>Ehc: persist ehc.hmac.secret
  App->>Ehc: ehc_tel_enqueue / tick
  Ehc->>Dev: POST telemetry HMAC
  Tel-->>Dev: 202 + pendingCommands
  Dev->>Tel: POST executed HMAC
  Dev->>App: on_command / deferred
```

---

## Headers exactos

```
Content-Type: application/json\r\n
x-edgehub-device-signature: <64 hex>\r\n
```

HMAC = HMAC-SHA256(secret, canonical_utf8) em hex minúsculo. Se `strlen(secret) > 64`, a chave HMAC é `SHA256(secret)` (RFC 2104).

Retry previous: só se HTTP 401 **e** o body contém o token `DEVICE_CREDENTIAL_INVALID` **e** existe `secret_previous`. **Não** retry em EXPIRED/REVOKED.

---

## O que esta lib não chama

- `GET {downloadUrl}` — aplicação.
- `POST …/media/public/…/streams/publish-session` — não está no C.
- `GET /commands/internal/executions/pending` — API interna com token de serviço, não HMAC de device.
- Qualquer path com `base_url` já contendo `/api/v1`.
