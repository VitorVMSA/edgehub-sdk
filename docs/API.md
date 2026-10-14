# API ehc

Cabeçalhos públicos em `include/edgehub_client/`. Tabelas **não** entram na `.a` — cada binário traz as suas (`user_template/` é o molde). Camadas: [ARCHITECTURE.md](ARCHITECTURE.md). Flags: `disable_*` (memset 0 = master ligado). Integração: [INTEGRATION.md](INTEGRATION.md).

## Códigos `ehc_status_t`

| Valor | Nome | Significado |
|------:|------|-------------|
| 0 | `EHC_OK` | sucesso |
| -1 | `EHC_EINVAL` | argumento NULL / `ehc_init` sem `http_post` ou `now_ms` / não inicializado |
| -2 | `EHC_ENOTREADY` | prontidão insuficiente; `disable_telemetry` em `ehc_tel_*`; poll sem HMAC |
| -3 | `EHC_ECONFIG` | tabela inválida, três `disable_*`, `device.force_checkin` na tabela user, limites `EHC_MAX_*` |
| -4 | `EHC_EIO` | `eh_client_*` falhou (HTTP, parse, init device) |
| -5 | `EHC_EABI` | `eh_lib_abi_check` falhou |
| -6 | `EHC_EAGAIN` | reservado (não usado no caminho feliz actual) |

Device `eh_status_t` (interno): `EH_OK=0`, `EH_ERR_INVALID_ARG=-1`, `EH_ERR_NOT_INITIALIZED=-2`, `EH_ERR_HTTP=-3`, `EH_ERR_IO=-4`, `EH_ERR_PARSE=-5`, `EH_ERR_AUTH=-6`, `EH_ERR_BUFFER_TOO_SMALL=-7`.

## Identidade `ehc_identity_source_t`

```c
typedef struct {
  ehc_ident_kind_t kind; /* LITERAL=0, STORAGE=1, FN=2 */
  const char *value;
  int (*fn)(char *out, size_t cap);
} ehc_identity_source_t;
```

| Macro | Resolução |
|-------|-----------|
| `EHC_IDENT_LIT("https://devices.example.com")` | string estática; vazio = ausente |
| `EHC_IDENT_STORE("identity_pkcs8")` | `storage_read` da chave |
| `EHC_IDENT_FN(read_product)` | `fn(out,cap)` devolve nonzero **e** `out[0]` |
| `EHC_IDENT_NONE` | literal `""` |

Tecto `EHC_IDENT_MAX` = 192 (PKCS#8 no init usa buffer 512 local).

## `ehc_user_t`

| Campo | Tipo | Default `memset 0` | Notas |
|-------|------|--------------------|-------|
| `base_url` | identity | vazio → UNCONFIGURED | origem **sem** `/api/v1` |
| `tenant_slug` | identity | vazio = path sem slug | |
| `device_uid` | identity | vazio → UNCONFIGURED | serial; firmware **não** envia alias |
| `group_key` | identity | vazio → MINIMAL | tem de vir **com** product |
| `product_code` | identity | vazio → MINIMAL | |
| `identity_pkcs8` | identity | vazio | se resolver, auto `enable_identity_credential_issue` |
| `checkin_keys` / `checkin_key_count` | tabela | 0 | |
| `metrics` / `metric_count` | tabela | 0 | slave pode 0 se `disable_telemetry` |
| `commands` / `command_count` | tabela | 0 | |
| `on_config_apply` | callback | NULL = só persistir versão | **sob `ehc_lock()`**; return 0 = não gravar versão |
| `on_download` | callback | NULL = só log | GET+SHA256 na app |
| `user_ctx` | void* | NULL | passado aos handlers |
| `telemetry_manual_only` | uint8 | 0 | tick **não** drena fila; replay histórico |
| `telemetry_batch_max` | uint8 | 0 → `EHC_TELEMETRY_BATCH_MAX` (1) | >1 envia `readings[]` |
| `enable_identity_credential_issue` | uint8 | 0 | força issue mesmo sem PKCS#8 no user |
| `disable_telemetry` | uint8 | 0 = ligado | slave = 1 |
| `disable_commands` | uint8 | 0 = ligado | |
| `disable_checkin` | uint8 | 0 = ligado | |
| `poll_commands` | uint8 | 0 = off | só com `disable_telemetry=1` e `disable_commands=0` |

**Invariante:** os três `disable_*` a 1 → `EHC_ECONFIG` + log `all disable_* flags set; device ABI would re-enable all`.

Não existem `enable_telemetry` etc. no user — zero-fill **não** pode desligar o master.

## `ehc_port_t`

Ver [PORTING.md](PORTING.md). `ehc_init` exige `http_post` e `now_ms` não-NULL.

## Ciclo de vida

```c
ehc_status_t ehc_init(const ehc_port_t *port, const ehc_user_t *user);
ehc_status_t ehc_set_log_level(ehc_log_level_t level);
void ehc_shutdown(void);              /* memset g_ehc */
int ehc_is_initialized(void);
ehc_status_t ehc_tick(void);
ehc_status_t ehc_force_checkin(void);
ehc_status_t ehc_poll_downlink(void); /* POST values:{} já */
int ehc_poll_commands_enabled(void);  /* user.poll_commands em runtime */
```

`ehc_init` **não** chama `eh_client_init` ainda (lazy no primeiro tick com prontidão ≥ MINIMAL).

### Wrappers runtime (lock + init se preciso)

**Não** chamar de `on_config_apply` (deadlock: apply já corre sob `ehc_lock`). Enfileirar para o loop da app.

```c
ehc_status_t ehc_set_checkin_interval(uint32_t ms);     /* 0 = default device 60 s — o client já passou 12 h no init */
ehc_status_t ehc_set_telemetry_batch_max(uint8_t n);
ehc_status_t ehc_set_telemetry_interval(uint32_t ms);   /* no-op útil se telemetry_manual_only */
ehc_status_t ehc_get_device_status(ehc_device_status_t *out);
```

`ehc_device_status_t`:

| Campo | Origem |
|-------|--------|
| `provisioning` | `eh_provisioning_status_t`: 0 unknown, 1 pending, 2 active |
| `next_checkin_at_ms` | Unix ms |
| `backoff_until_ms` | 0 = sem backoff |
| `pending_command_count` | 0..5 |

### Prontidão

```c
ehc_readiness_t ehc_get_readiness(void); /* UNCONFIGURED MINIMAL FULL ACTIVE */
const char *ehc_readiness_name(ehc_readiness_t r);
const char *ehc_not_ready_reason(void);
```

## Telemetria (API)

```c
ehc_status_t ehc_tel_set_num(const char *name, double value);
ehc_status_t ehc_tel_set_str(const char *name, const char *value);
ehc_status_t ehc_tel_set_bool(const char *name, int value);
ehc_status_t ehc_tel_set_num_in(const char *parent, const char *name, double value);
ehc_status_t ehc_tel_set_str_in(const char *parent, const char *name, const char *value);
ehc_status_t ehc_tel_set_bool_in(const char *parent, const char *name, int value);
ehc_status_t ehc_tel_set_num_at(const char *path, double value); /* "motor.rpm" */
ehc_status_t ehc_tel_enqueue(const char *ts_iso); /* NULL = now_ms */
ehc_status_t ehc_tel_flush(void);
uint8_t ehc_tel_queued(void);
void ehc_tel_reset(void);
void ehc_tel_queue_clear(void);
ehc_status_t ehc_tel_publish(void); /* enqueue(NULL)+flush */
```

Macros de tabela: `EHC_METRIC_NUM`, `_STR`, `_BOOL`, `_OBJECT`, `_IN`, `_GET`, `_GET_IN`, `_STALE`.

`EHC_METRIC_NUM_STALE(name)`, `EHC_METRIC_STR_STALE(name)`, `EHC_METRIC_BOOL_STALE(name)`: `send_when_stale=1`. Se nunca `filled` e sem getter, envia **0** / `""` / `false` (não omite).

Objecto sem filhos preenchidos é omitido. `set_*` marca `filled=1`. **Não** há auto-reset no `enqueue`. Escrever `""` num STR **inclui** o campo. Replay: sempre `reset` → `set_*` → `enqueue`.

## Check-in keys

```c
EHC_KEY_CONST("boardModel", "example_board")
EHC_KEY_FN("firmware", app_fw_version, EHC_KEY_REQUIRED)
EHC_KEY_ASSET("classifications")
EHC_KEY_ASSET_FLAGS("firmware", EHC_KEY_REQUIRED)
```

`EHC_KEY_REQUIRED = 1u`. Sem valor → UNCONFIGURED.

## Comandos

```c
EHC_COMMAND("motor.set_speed", cmd_set_speed)
EHC_COMMAND_FLAGS("device.reboot", cmd_reboot, EHC_CMD_DEFER_AFTER_ACK)
```

Accessors: `ehc_args_has/num/int/bool/str/raw`. Resultados: `ehc_cmd_ok` / `okf` / `fail` (devolvem 1/1/0 para o device).

`commandKey` ausente/`null`/`""` no JSON **ainda despacha** (`pending_cmd=?`). Handler user trata como desconhecido. Não declare `device.force_checkin`.

## Logs `ehc_log_level_t`

`ERROR=0 WARN=1 INFO=2 DEBUG=3`. Port `log` opcional.

## Nível de log

`ehc_set_log_level()` configura ERROR/WARN/INFO/DEBUG em runtime. O padrão após `ehc_init()` é `EHC_LOG_INFO`; `EHC_LOG_DEBUG` ativa os eventos detalhados documentados em [INTEGRATION.md](INTEGRATION.md).

## Device SDK (quem consome `device/` directo)

API: `eh_client_init/tick/force_checkin/force_telemetry/poll_downlink`, `eh_values_*`, `eh_client_set_secret`, `eh_client_set_identity_private_key_pkcs8_b64`, `eh_lib_abi_check`. Callbacks em `eh_callbacks_t`. Não misturar `ed25519_random_host.c` e `ed25519_random_embedded.c` no mesmo binário. Ver `device/docs/INTEGRATION.md`.

## Exemplo mínimo

```c
memset(&user, 0, sizeof(user));
user.base_url = (ehc_identity_source_t)EHC_IDENT_LIT("https://devices.example.com");
user.tenant_slug = (ehc_identity_source_t)EHC_IDENT_LIT("my-tenant");
user.device_uid = (ehc_identity_source_t)EHC_IDENT_FN(read_serial);
user.group_key = (ehc_identity_source_t)EHC_IDENT_FN(read_group);
user.product_code = (ehc_identity_source_t)EHC_IDENT_LIT("myProduct");
user.checkin_keys = ehc_user_checkin_keys;
user.checkin_key_count = ehc_user_checkin_keys_count;
user.metrics = ehc_user_metrics;
user.metric_count = ehc_user_metrics_count;
user.commands = ehc_user_commands;
user.command_count = ehc_user_commands_count;
user.on_config_apply = ehc_user_on_config_apply;
user.on_download = ehc_user_on_download;
user.identity_pkcs8 = (ehc_identity_source_t)EHC_IDENT_STORE("identity_pkcs8");
ehc_init(&port, &user);
```

Slave (heartbeat de comandos, sem métricas):

```c
user.disable_telemetry = 1;
user.poll_commands = 1;
user.metric_count = 0;
```
