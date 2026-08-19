# Integração

Copie `user_template/` para o firmware. Não ligue esses `.c` à biblioteca — cada binário traz as suas tabelas.

## 1. Config compile-time

`ehc_user_config.h` define limites (`EHC_MAX_NESTING_DEPTH`, pool de strings, intervalos). A lib usa `include/edgehub_client/ehc_config_defaults.h` quando o header do utilizador não entra no compile da `.a`.

Para aplicar limites custom na lib:

```
-DEHC_USER_CONFIG_HEADER="ehc_user_config.h"
```

e o include path do template. `ehc_init` recusa tabelas que excedam os limites (`EHC_ECONFIG`) e loga o elemento.

## 2. Identidade

```c
u.base_url    = (ehc_identity_source_t)EHC_IDENT_LIT("https://tenant.edgehub.com.br");
u.device_uid  = (ehc_identity_source_t)EHC_IDENT_LIT("UID");
u.group_key   = (ehc_identity_source_t)EHC_IDENT_STORE("group_key"); /* ou LIT / FN */
u.product_code= (ehc_identity_source_t)EHC_IDENT_FN(read_product);
```

Sem `base_url`+`device_uid` (e keys `EHC_KEY_REQUIRED`) o tick devolve `EHC_ENOTREADY` e **não** chama `eh_client_init`.

Com `group_key`+`product_code` o check-in passa a corpo completo (`eh_client_set_group_product`). Telemetria só é enviada em `ACTIVE` (provisioning `active` + segredo HMAC). Antes disso `ehc_tel_publish` só enfileira.

## 3. Telemetria

```c
EHC_METRIC_NUM("temperature"),
EHC_METRIC_OBJECT("motor"),
EHC_METRIC_NUM_IN("motor", "rpm"),
EHC_METRIC_NUM_GET("uptimeSec", app_read_uptime),
```

Push: `ehc_tel_set_num` / `_in` / `_at`. Getter ganha sobre o cache de push. Objecto sem filhos preenchidos é omitido. Nomes de objecto únicos em toda a tabela.

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

`ehc_tel_publish()` força enqueue+flush (flush só em ACTIVE). `now_ms` vem do port.
