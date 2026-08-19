# Integração

Copie `user_template/` para o firmware. Não ligue esses `.c` à biblioteca — cada binário traz as suas tabelas.

Requer `edgehub-device-sdk` ≥ **1.2.2** (ABI 2). Client SDK **0.2.1**.

## 1. Config compile-time

`ehc_user_config.h` define limites (`EHC_MAX_NESTING_DEPTH`, pool de strings, intervalos). A lib usa `include/edgehub_client/ehc_config_defaults.h` quando o header do utilizador não entra no compile da `.a`.

Para aplicar limites custom na lib:

```
-DEHC_USER_CONFIG_HEADER="ehc_user_config.h"
```

e o include path do template. `ehc_init` recusa tabelas que excedam os limites (`EHC_ECONFIG`) e loga o elemento.

Payload JSON grande (EcoCircuito ~4 KiB): compile as **fontes** do device SDK com `-DEH_MAX_TELEMETRY_QUEUE=8` e `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096` (fila em `eh_client_t` ~33 KB). **`EH_MAX_BATCH_PAYLOAD` fica 64 KiB** — não descer para 32 KiB; o body de lote ~34 KB. Os defaults públicos do device SDK (16 / 768 / 64 KiB) **não** mudam. O `.a` pré-compilado com o default 768 **não** serve. `EHC_OBJECT_SCRATCH_BYTES` no template (256) também tem de acompanhar o JSON real — isso fica no glue do firmware.

## 2. Identidade

```c
u.base_url    = (ehc_identity_source_t)EHC_IDENT_LIT("https://tenant.edgehub.com.br");
u.device_uid  = (ehc_identity_source_t)EHC_IDENT_LIT("UID");
u.group_key   = (ehc_identity_source_t)EHC_IDENT_STORE("group_key"); /* ou LIT / FN */
u.product_code= (ehc_identity_source_t)EHC_IDENT_FN(read_product);
```

Sem `base_url`+`device_uid` (e keys `EHC_KEY_REQUIRED`) o tick devolve `EHC_ENOTREADY` e **não** chama `eh_client_init`.

Com `group_key`+`product_code` o check-in passa a corpo completo (`eh_client_set_group_product`). Telemetria só é enviada em `ACTIVE` (provisioning `active` + segredo HMAC). Antes disso `ehc_tel_enqueue` / `ehc_tel_publish` só enfileiram; `ehc_tel_flush` não POST.

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

## 4. Check-in keys e assets

```c
EHC_KEY_CONST("boardModel", "ecopro_v3"),
EHC_KEY_FN("firmware", app_fw_version, EHC_KEY_REQUIRED),
EHC_KEY_ASSET("classifications"),
```

`on_config_apply` corre no check-in; a versão só é gravada em `ehc.asset.<nome>` se o apply devolver sucesso.

## 5. Comandos

```c
EHC_COMMAND("motor.set_speed", cmd_set_speed),
EHC_COMMAND_FLAGS("device.reboot", cmd_reboot, EHC_CMD_DEFER_AFTER_ACK),
```

Accessors: `ehc_args_int/num/bool/str/has/raw`. Não declare `device.force_checkin` — é built-in do device SDK. `DEFER_AFTER_ACK` confirma o `executed` antes de correr o handler (reboot/OTA).

## 6. Loop

```c
ehc_init(&port, &user);
for (;;) {
  ehc_tick();
}
```

`now_ms` vem do port. Replay histórico chama `ehc_tel_reset` / `ehc_tel_enqueue` / `ehc_tel_flush` (e `ehc_tel_queue_clear` se o flush falhar) no próprio loop; o tick fica para check-in e comandos.
