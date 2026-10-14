# Integração

Copie `user_template/` para o firmware. Não ligue esses `.c` à biblioteca — cada binário traz as suas tabelas.

Umbrella **0.3.3**. Protocolo in-tree: `device/` = edgehub-device-sdk **1.2.6** (ABI 2). Boot esperado: `lib=1.2.6 client=0.3.3 abi=2`.

Um submodule para todos os firmwares (mapa em `releases/v0.3.2/SYNC.txt`). No slave ESP-IDF **não** usar `components/` (o `project()` host desta lib parte o configure):

```
git submodule add git@github.com-vmsatech:VitorVMSA/edgehub-client-sdk.git third_party/edgehub-client-sdk
```

CMake do firmware:

- Device: `EH_SDK_DIR` = `../edgehub_sdk/device` (ou `third_party/edgehub-client-sdk/device`).
- Client: fontes `src`, `include`, `port/esp32`.
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

HMAC: o servidor aceita o secret antigo até `rotateGraceUntil`. Após 401 `*_CREDENTIAL_INVALID` a lib faz **um** retry com `secret_previous`. `expiresAt` do issue é parseado; no tick, depois dessa data, arma `needs_credential_reprovision` (check-in sem HMAC, como no 401 EXPIRED). A comparação usa `now_ms` do port: precisa de wall-clock Unix. O port ESP32 usa `gettimeofday` (epoch); SNTP fica no firmware. Relógio só `esp_timer` (uptime) não dispara este caminho — a recuperação continua a ser o 401 EXPIRED.

### Logs e diagnóstico seguro

O callback de log já existente no port (`ehc_port_t.log`) recebe os logs do client e do device. O nível padrão continua `EHC_LOG_INFO`. Para uma sessão de diagnóstico, configure DEBUG logo após `ehc_init` e antes de `ehc_tick`, `ehc_tel_enqueue` ou `ehc_tel_flush`:

```c
ehc_status_t rc = ehc_init(&port, &user);
if (rc == EHC_OK) {
  (void)ehc_set_log_level(EHC_LOG_DEBUG);
}
```

`ehc_set_log_level()` aceita `EHC_LOG_ERROR`, `EHC_LOG_WARN`, `EHC_LOG_INFO` e `EHC_LOG_DEBUG`; valor fora da enumeração devolve `EHC_EINVAL`. A chamada também atualiza o device se este já tiver sido inicializado. Não altera a assinatura nem os argumentos do callback e não exige mudanças nos ports host, Linux, ESP32 ou embedded.

Em DEBUG, os eventos `telemetry_enqueue` e `telemetry_flush` do client incluem etapa, `status` simbólico e numérico (`EHC_*`), `queue_before`, `queue_after` e `result`. O device emite `telemetry_http` com `result`, `http_status` e `category`: `transport_failure`, `request_failure` (preparação local antes do HTTP), `http_rejection` ou `accepted`; categorias `credential` e `validation` só são usadas para valores exatos do campo JSON `code` que a implementação reconhece. Outros corpos de erro ficam como `unknown`. O evento de sucesso HTTP é evidência da resposta 2xx; não prova processamento posterior do lado do servidor.

Para coletar em ambiente de teste, use um dispositivo de bancada e um callback de log que encaminhe mensagens para o console/serial do port. Ative DEBUG temporariamente, reproduza um enqueue e flush, e recolha o intervalo desde `telemetry_enqueue` até `telemetry_http`/`telemetry_flush`. Os eventos não imprimem URL, headers, corpo, valores de métricas ou credenciais. Não adicione dumps de request/response ao callback.

`ehc_tel_flush()` continua a devolver apenas `ehc_status_t`: HTTP/transporte falho chega ao client como `EHC_EIO`. Para medir remoção, compare `ehc_tel_queued()` antes e depois. HTTP 2xx indica sucesso da requisição e o device SDK remove do RAM os itens enviados. A contagem da fila não identifica os `rec_id` correspondentes, e em falha `rejectedIndex` pode remover uma leitura sem que a API pública revele qual. Portanto, HTTP 2xx + redução da fila não é ACK persistente por registro. A aplicação deve manter replay/ACK sob sua responsabilidade e definir uma política de duplicidade apropriada. `EHC_OK` sozinho também não confirma envio: antes de ACTIVE, o flush pode retornar OK mantendo a fila.

A fila do SDK é RAM. Persistência, replay e ACK do armazenamento pertencem à aplicação consumidora. Quando a fila RAM atinge a capacidade, a política existente remove a leitura mais antiga e aceita a nova (`telemetry_queue_full_drop_oldest`). Isso é saturação local, não rejeição remota.

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
  /* Não dê ACK persistente; a API não identifica o rec_id rejeitado. */
}
```

`ehc_tel_flush` devolve só `ehc_status_t`. `sent = queued_antes - queued_depois` é uma contagem de remoções da fila RAM. Em 2xx, o device SDK remove os itens enviados; em falha HTTP, `rejectedIndex` pode remover um item, mas a API pública não expõe seu índice nem identifica os `rec_id`. Assim, a aplicação não consegue garantir ACK persistente individual apenas com estas funções. Não use `ehc_tel_queue_clear()` como tratamento genérico. Saturação da fila RAM é local, não rejeição remota. `ehc_tick()` **não** envia telemetria quando `telemetry_manual_only=1`.

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

Poll imediato (além do intervalo no tick): `ehc_poll_downlink()`. O tick **volta a ler** `ehc_now()` imediatamente antes do POST — o check-in do mesmo ciclo pode ter sincronizado o relógio via HTTP `Date`. `ehc_poll_commands_enabled()` reflecte `user.poll_commands` em runtime.

## 6. Loop

```c
ehc_init(&port, &user);
for (;;) {
  ehc_tick();
}
```

`now_ms` vem do port. Replay histórico chama `ehc_tel_reset` / `ehc_tel_enqueue` / `ehc_tel_flush` no próprio loop; o tick fica para check-in e comandos. Não use `ehc_tel_queue_clear` automaticamente em falha: `rejectedIndex` pode remover uma leitura sem identificar seu `rec_id` na API pública.

Wrappers runtime (lock + `eh_client_init` se ainda não houver). **Não** chamar de `on_config_apply`:

```c
ehc_set_checkin_interval(ms);
ehc_set_telemetry_batch_max(n);
ehc_set_telemetry_interval(ms); /* no-op útil se telemetry_manual_only */
ehc_poll_downlink(); /* slave: POST {values:{}} já, se HMAC existir */
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
