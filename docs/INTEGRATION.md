# Integração

Copie `user_template/` para o firmware. Não ligue esses `.c` à biblioteca — cada binário traz as suas tabelas.

Umbrella **0.3.1**. Protocolo in-tree: `device/` = edgehub-device-sdk **1.2.5** (ABI 2). Boot esperado: `lib=1.2.5 client=0.3.1 abi=2`.

Um submodule para todos os firmwares (mapa em `releases/v0.3.1/SYNC.txt`):

```
git submodule add git@github.com-vmsatech:VitorVMSA/edgehub-client-sdk.git components/edgehub_sdk
```

CMake do firmware (não alterado nesta entrega):

- Device: `EH_SDK_DIR` = `../edgehub_sdk/device` (hoje `sdk/` no EcoCircuito).
- Client: fontes `../edgehub_sdk/src`, `include`, `port/esp32`.
- Overlay EcoCircuito inalterado (`overlay/` / fallback `config.apply`).

## 1. Config compile-time

`ehc_user_config.h` define limites (`EHC_MAX_NESTING_DEPTH`, pool de strings, intervalos). A lib usa `include/edgehub_client/ehc_config_defaults.h` quando o header do utilizador não entra no compile da `.a`.

Para aplicar limites custom na lib:

```
-DEHC_USER_CONFIG_HEADER="ehc_user_config.h"
```

e o include path do template. `ehc_init` recusa tabelas que excedam os limites (`EHC_ECONFIG`) e loga o elemento.

`g_ehc` pode ir a PSRAM no firmware (pool de strings grande) sem patchar `ehc_core.c`:

```
#define EHC_CTX_ATTR __attribute__((section(".ext_ram.bss")))
```

em `ehc_user_config.h` (só `ESP_PLATFORM`). Sem o define, BSS normal.

Payload JSON grande (EcoCircuito ~4 KiB): compile as **fontes** de `device/` com `-DEH_MAX_TELEMETRY_QUEUE=8` e `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096` (fila em `eh_client_t` ~33 KB). **`EH_MAX_BATCH_PAYLOAD` fica 64 KiB** — não descer para 32 KiB; o body de lote ~34 KB. Os defaults públicos do device SDK (16 / 768 / 64 KiB) **não** mudam. O `.a` pré-compilado com o default 768 **não** serve. `EHC_OBJECT_SCRATCH_BYTES` no template (256) também tem de acompanhar o JSON real — isso fica no glue do firmware.

## 2. Identidade

```c
u.base_url    = (ehc_identity_source_t)EHC_IDENT_LIT("https://tenant.edgehub.com.br");
u.device_uid  = (ehc_identity_source_t)EHC_IDENT_LIT("UID");
u.group_key   = (ehc_identity_source_t)EHC_IDENT_STORE("group_key"); /* ou LIT / FN */
u.product_code= (ehc_identity_source_t)EHC_IDENT_FN(read_product);
```

Sem `base_url`+`device_uid` (e keys `EHC_KEY_REQUIRED`) o tick devolve `EHC_ENOTREADY` e **não** chama `eh_client_init`.

Com `group_key`+`product_code` o check-in passa a corpo completo (`eh_client_set_group_product`). Telemetria só é enviada em `ACTIVE` (provisioning `active` + segredo HMAC). Antes disso `ehc_tel_enqueue` / `ehc_tel_publish` só enfileiram; `ehc_tel_flush` não POST.

PKCS#8 Ed25519 (`identity_pkcs8`) dispara `POST .../device/credential/issue` quando o check-in traz `identityChallenge`. O client liga `enable_identity_credential_issue` sozinho se o PKCS#8 resolver (literal/storage/fn). A flag `enable_identity_credential_issue` continua a poder forçar o caminho sem PKCS#8 preenchido no `ehc_user_t`.

```c
u.identity_pkcs8 = (ehc_identity_source_t)EHC_IDENT_STORE("identity_pkcs8");
/* ou u.enable_identity_credential_issue = 1; */
```

HMAC: o servidor aceita o secret antigo até `rotateGraceUntil`. Após 401 `*_CREDENTIAL_INVALID` a lib faz **um** retry com `secret_previous`. `expiresAt` do issue é parseado; no tick, depois dessa data, arma `needs_credential_reprovision` (check-in sem HMAC, como no 401 EXPIRED). A comparação usa `now_ms` do port: precisa de wall-clock Unix (Linux `CLOCK_REALTIME`). Relógio monotónico (ESP32 `esp_timer`) não dispara este caminho — a recuperação continua a ser o 401 EXPIRED.

## 3. Telemetria

O client **liga ao EdgeHub, mantém a sessão e envia um ou mais pacotes** que a aplicação enfileirou. **Não** é fila durável. Pacotes em flash/disco ficam no `telemetry_store` (ou equivalente) da aplicação. A fila RAM **não** vai à NVS (`telemetry_queue_persist = 0`; o bridge ignora `eh.tel.q`). HMAC e versões de asset continuam no storage do port.

```c
EHC_METRIC_NUM("temperature"),
EHC_METRIC_OBJECT("motor"),
EHC_METRIC_NUM_IN("motor", "rpm"),
EHC_METRIC_NUM_GET("uptimeSec", app_read_uptime),
```

Push: `ehc_tel_set_num` / `_in` / `_at`. Getter ganha sobre o cache de push. Objecto sem filhos preenchidos é omitido. Nomes de objecto únicos em toda a tabela. `set_*` marca `filled=1`; **não** há auto-reset no `enqueue`.

### Sensores vivos

`ehc_tel_publish()` = `ehc_tel_enqueue(NULL)` + `ehc_tel_flush()` (um POST por pacote; `ts` = relógio do port). Com `telemetry_manual_only=0`, `ehc_tick()` também drena a fila.

### Replay histórico (lote)

Cada leitura vem de um registo (não de getters vivos). Loop por rec: **`reset` → `set_*` → `enqueue`**. Sem `reset`, campos condicionais (`lastWasteTags` só em `NEW_WASTE`) contaminam as leituras seguintes. Escrever `""` muda o contrato (campo presente vs omitido).

```c
u.telemetry_manual_only = 1;
u.telemetry_batch_max = 8; /* 0 = EHC_TELEMETRY_BATCH_MAX (default 1 = corpo legado) */

for (/* cada rec_id pendente */) {
  ehc_tel_reset();
  /* preencher cache a partir do telemetry_record_t */
  ehc_tel_set_*(...);
  ehc_tel_enqueue(iso8601_utc_de(rec->timestamp_s)); /* não now */
}
before = ehc_tel_queued();
rc = ehc_tel_flush(); /* um POST readings[] se batch_max > 1 */
sent = (uint8_t)(before - ehc_tel_queued());
if (rc != EHC_OK) {
  ehc_tel_queue_clear(); /* store reenvia os rec_id; não ack sent */
}
```

`ehc_tel_flush` devolve só `ehc_status_t`. Contagem: `sent = queued_antes - queued_depois` **só** no caminho OK. Em falha HTTP a fila RAM fica; `rejectedIndex` num 401 pode tirar uma leitura do meio — sem `queue_clear` o store duplica `rec_id`. `ehc_tick()` **não** envia telemetria quando `telemetry_manual_only=1`. `ehc_tel_queued()` devolve o tamanho da fila.

Números no `values` / `readings[]` seguem `JSON.stringify` (device 1.2.5). O servidor HMAC-a `JSON.stringify` do body parseado; `%.15g` (`1e-05`) falha o ingest com 401 `TELEMETRY_DEVICE_CREDENTIAL_INVALID` mesmo com o segredo válido. Não imprimir métricas à mão — `ehc_tel_set_num` / `eh_values_add_num`.

## 4. Check-in keys e assets

```c
EHC_KEY_CONST("boardModel", "ecopro_v3"),
EHC_KEY_FN("firmware", app_fw_version, EHC_KEY_REQUIRED),
EHC_KEY_ASSET("classifications"),
```

`on_config_apply` corre no check-in **sob `ehc_lock()`**. A versão só é gravada em `ehc.asset.<nome>` se o apply devolver sucesso. **Não** chame `ehc_set_checkin_interval` / `ehc_set_telemetry_*` de dentro do apply — enfileire o trabalho para o loop da aplicação.

`on_download` corre no mesmo check-in para cada entrada em `downloads` (`version` + `downloadUrl` obrigatórios; `checksumSha256` pode vir vazio). NULL = só log (sem OTA). O HAL do port **não** tem `http_get`: o GET de `downloadUrl` e a validação SHA-256 (se o checksum existir) são da aplicação, como o guia EdgeHub.

```c
u.on_download = ehc_user_on_download;
```

Configurações: `payload` é obrigatório; `source` ausente chega como `""`.

## 5. Comandos

```c
EHC_COMMAND("motor.set_speed", cmd_set_speed),
EHC_COMMAND_FLAGS("device.reboot", cmd_reboot, EHC_CMD_DEFER_AFTER_ACK),
```

Accessors: `ehc_args_int/num/bool/str/has/raw`. Não declare `device.force_checkin` — é built-in do device SDK. `DEFER_AFTER_ACK` confirma o `executed` antes de correr o handler (reboot/OTA).

**Contrato EdgeHub (não há poll público de comandos):**

- Downlink = array `pendingCommands` na resposta **2xx** de `POST /device/telemetry`.
- Confirmação = `POST .../device/command-executions/{executionId}/executed` (HMAC; o client faz isto com `enable_auto_confirm_commands=1`).
- O check-in MDM **não** devolve `pendingCommands`. `GET /commands/internal/executions/pending` é API interna (token de serviço), não HMAC de dispositivo.
- Slave com `disable_telemetry=1` **não recebe** reboot/`force_checkin` pelo MDM, salvo `poll_commands=1` (abaixo).
- O ingest de telemetria (incluindo o heartbeat vazio) conta crédito `telemetry_events`.
- Cap alinhado com o servidor: `EH_MAX_COMMANDS` = 5. Um 6.º na mesma resposta é ignorado (log `pending_commands_truncated`).

Heartbeat opt-in no **mesmo** ingest (não inventa URL):

```c
u.disable_telemetry = 1;
u.poll_commands = 1; /* POST {deviceUid, ts, values:{}} a cada EHC_COMMAND_POLL_INTERVAL_MS (default 60 s) */
```

`memset` 0 = off. Não liga `enable_telemetry` por baixo (o collect de métricas não corre).

## 6. Loop

```c
ehc_init(&port, &user);
for (;;) {
  ehc_tick();
}
```

`now_ms` vem do port. Replay histórico chama `ehc_tel_reset` / `ehc_tel_enqueue` / `ehc_tel_flush` (e `ehc_tel_queue_clear` se o flush falhar) no próprio loop; o tick fica para check-in e comandos.

Wrappers runtime (lock + `eh_client_init` se ainda não houver). **Não** chamar de `on_config_apply`:

```c
ehc_set_checkin_interval(ms);
ehc_set_telemetry_batch_max(n);
ehc_set_telemetry_interval(ms); /* no-op útil se telemetry_manual_only */
ehc_device_status_t st;
ehc_get_device_status(&st); /* provisioning, next_checkin, backoff, pending_command_count */
```

## 7. Features a menos (`disable_*`)

`memset(&user, 0)` continua a significar **master actual**: check-in + telemetria + comandos ligados. Os campos são `disable_*` (não `enable_*=0`), para o zero-fill não desligar nada.

```c
u.disable_telemetry = 1; /* slave: sem POST /device/telemetry */
u.metric_count = 0;       /* tabela vazia permitida */
u.disable_commands = 1;   /* opcional; command_count pode ser 0 */
u.disable_checkin = 1;    /* opcional */
```

- `disable_telemetry`: não valida a tabela se `metric_count == 0`; `ehc_tel_*` devolve `EHC_ENOTREADY`; `ehc_tick` não faz flush. O device recebe `enable_telemetry=0`. Sem `poll_commands`, **não há** POST `/device/telemetry` e portanto **não há** `pendingCommands`.
- `disable_commands`: não valida a tabela se `command_count == 0`.
- RAM continua a ser CMake do firmware (`EH_MAX_TELEMETRY_QUEUE=1` no slave, `8`/`4096` no master; `EHC_TEL_STR_MAX`).

`ehc_init` recusa os **três** `disable_*` a 1 (`EHC_ECONFIG` + log): o device SDK (ABI 2) volta a ligar check-in, telemetria e comandos se os três `enable_*` forem 0.
