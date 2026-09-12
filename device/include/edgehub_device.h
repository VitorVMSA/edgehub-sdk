#ifndef EDGEHUB_DEVICE_H
#define EDGEHUB_DEVICE_H

#include <stddef.h>
#include <stdint.h>

/** Semantic version of edgehub-device-sdk (must match linked .a). */
#define EH_LIB_VERSION_MAJOR 1
#define EH_LIB_VERSION_MINOR 2
#define EH_LIB_VERSION_PATCH 6
#define EH_LIB_ABI_VERSION   2

#define EH_LIB_VERSION_STRINGIFY_(x) #x
#define EH_LIB_VERSION_STRINGIFY(x) EH_LIB_VERSION_STRINGIFY_(x)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef EH_EXPORTS
#define EH_API __declspec(dllexport)
#else
#define EH_API
#endif
#else
#define EH_API
#endif

#ifndef EH_MAX_URL
#define EH_MAX_URL 192
#endif

#ifndef EH_MAX_TENANT_SLUG
#define EH_MAX_TENANT_SLUG 64
#endif

#ifndef EH_MAX_DEVICE_UID
#define EH_MAX_DEVICE_UID 96
#endif

#ifndef EH_MAX_ALIAS
#define EH_MAX_ALIAS 96
#endif

#ifndef EH_MAX_GROUP_KEY
#define EH_MAX_GROUP_KEY 128
#endif

#ifndef EH_MAX_PRODUCT_CODE
#define EH_MAX_PRODUCT_CODE 96
#endif

#ifndef EH_MAX_SECRET
#define EH_MAX_SECRET 128
#endif

#ifndef EH_MAX_PAYLOAD
#define EH_MAX_PAYLOAD 4096
#endif

#ifndef EH_MAX_RESPONSE
#define EH_MAX_RESPONSE 8192
#endif

#ifndef EH_MAX_HEADERS
#define EH_MAX_HEADERS 768
#endif

#ifndef EH_MAX_EXEC_ID
#define EH_MAX_EXEC_ID 96
#endif

#ifndef EH_MAX_COMMANDS
/** Fila local em `eh_client_t` (~1,2 KiB por slot). O 6.º comando na mesma resposta é ignorado (log). */
#define EH_MAX_COMMANDS 5
#endif

#ifndef EH_MAX_COMMAND_KEY
#define EH_MAX_COMMAND_KEY 128
#endif

#ifndef EH_MAX_COMMAND_PAYLOAD
#define EH_MAX_COMMAND_PAYLOAD 1024
#endif

#ifndef EH_MAX_DEVICE_MESSAGE
#define EH_MAX_DEVICE_MESSAGE 2000
#endif

#ifndef EH_MAX_TS
#define EH_MAX_TS 48
#endif

#ifndef EH_MAX_TELEMETRY_QUEUE
/** Leituras em RAM para drenagem offline / lote. ~EH_MAX_TELEMETRY_QUEUE_VALUES+EH_MAX_TS por slot. */
#define EH_MAX_TELEMETRY_QUEUE 16
#endif

#ifndef EH_MAX_TELEMETRY_QUEUE_VALUES
#define EH_MAX_TELEMETRY_QUEUE_VALUES 768
#endif

#ifndef EH_MAX_BATCH_PAYLOAD
/** Body + canónico HMAC de um lote (heap). */
#define EH_MAX_BATCH_PAYLOAD 65536
#endif

#ifndef EH_DEFAULT_TELEMETRY_BATCH_MAX
#define EH_DEFAULT_TELEMETRY_BATCH_MAX 1
#endif

#ifndef EH_MAX_CHECKIN_KEYS
#define EH_MAX_CHECKIN_KEYS 768
#endif

#ifndef EH_MAX_CHECKIN_ASSET_KEY
#define EH_MAX_CHECKIN_ASSET_KEY 128
#endif

#ifndef EH_MAX_CHECKIN_VERSION
#define EH_MAX_CHECKIN_VERSION 64
#endif

#ifndef EH_MAX_DOWNLOAD_URL
#define EH_MAX_DOWNLOAD_URL 384
#endif

#ifndef EH_MAX_CHECKSUM_SHA256
#define EH_MAX_CHECKSUM_SHA256 65
#endif

#ifndef EH_MAX_CHECKIN_CONFIG_SOURCE
#define EH_MAX_CHECKIN_CONFIG_SOURCE 64
#endif

#ifndef EH_MAX_CHECKIN_CONFIG_PAYLOAD
/** Payload JSON por asset configuration (objecto `payload`). */
#define EH_MAX_CHECKIN_CONFIG_PAYLOAD 4096
#endif

#ifndef EH_MAX_CHECKIN_CONFIG_ENTRY
/** Entrada completa `{version,source,payload}` em `configurations.<key>`. */
#define EH_MAX_CHECKIN_CONFIG_ENTRY 4096
#endif

#ifndef EH_MAX_ED25519_SIG_B64
#define EH_MAX_ED25519_SIG_B64 128
#endif

typedef enum {
  EH_OK = 0,
  EH_ERR_INVALID_ARG = -1,
  EH_ERR_NOT_INITIALIZED = -2,
  EH_ERR_HTTP = -3,
  EH_ERR_IO = -4,
  EH_ERR_PARSE = -5,
  EH_ERR_AUTH = -6,
  EH_ERR_BUFFER_TOO_SMALL = -7,
} eh_status_t;

typedef enum {
  EH_LOG_ERROR = 0,
  EH_LOG_WARN = 1,
  EH_LOG_INFO = 2,
  EH_LOG_DEBUG = 3,
} eh_log_level_t;

typedef enum {
  EH_PROV_UNKNOWN = 0,
  EH_PROV_PENDING = 1,
  EH_PROV_ACTIVE = 2,
} eh_provisioning_status_t;

typedef struct {
  char execution_id[EH_MAX_EXEC_ID];
  char command_key[EH_MAX_COMMAND_KEY];
  char payload_json[EH_MAX_COMMAND_PAYLOAD];
} eh_pending_command_t;

typedef struct {
  char ts[EH_MAX_TS];
  char values_json[EH_MAX_TELEMETRY_QUEUE_VALUES];
} eh_queued_reading_t;

typedef struct {
  char *buf;
  size_t cap;
  size_t len;
  uint8_t started;
  uint8_t has_any;
} eh_values_writer_t;

typedef struct {
  eh_provisioning_status_t provisioning;
  uint64_t next_checkin_at_ms;
  uint64_t next_telemetry_at_ms;
  uint64_t backoff_until_ms;
  uint32_t backoff_current_ms;
  uint8_t pending_command_count;
  uint8_t initialized;
} eh_client_status_t;

EH_API eh_status_t eh_values_begin(eh_values_writer_t *w, char *buf, size_t cap);
EH_API eh_status_t eh_values_add_str(eh_values_writer_t *w, const char *key, const char *value);
EH_API eh_status_t eh_values_add_num(eh_values_writer_t *w, const char *key, double value);
EH_API eh_status_t eh_values_add_bool(eh_values_writer_t *w, const char *key, int value);
/** Inserts `key` with an already-serialized JSON value (object, array, number, …). */
EH_API eh_status_t eh_values_add_raw(eh_values_writer_t *w, const char *key, const char *raw_json);
EH_API eh_status_t eh_values_end(eh_values_writer_t *w);

typedef struct {
  char base_url[EH_MAX_URL];
  char tenant_slug[EH_MAX_TENANT_SLUG];
  char device_uid[EH_MAX_DEVICE_UID];
  /** @deprecated Reservado; alias é atribuído pela plataforma — dispositivos não enviam. */
  char alias[EH_MAX_ALIAS];
  char group_key[EH_MAX_GROUP_KEY];
  char product_code[EH_MAX_PRODUCT_CODE];
  char telemetry_ts[EH_MAX_TS];

  uint8_t enable_logs;
  eh_log_level_t log_level;

  uint8_t enable_checkin;
  uint8_t enable_telemetry;
  /** 1 = tick não envia telemetria; use eh_client_force_telemetry. */
  uint8_t telemetry_manual_only;
  uint8_t enable_commands;
  /** Após on_command, POST .../executed automaticamente (requer credencial HMAC). */
  uint8_t enable_auto_confirm_commands;
  /** 1 = check-in imediato quando keys reportadas mudam vs último envio (ignora backoff). */
  uint8_t checkin_on_keys_change;

  uint32_t checkin_interval_ms;
  uint32_t telemetry_interval_ms;
  uint32_t backoff_initial_ms;
  uint32_t backoff_max_ms;

  /** Emite credencial HMAC via identity challenge Ed25519 após check-in. */
  uint8_t enable_identity_credential_issue;
  /**
   * Máx. de leituras por POST. 0 = default 1 (corpo single, servidores antigos).
   * >1 envia `readings[]` (requer ingest em lote no servidor).
   */
  uint8_t telemetry_batch_max;
  /** Capacidade da fila offline (0 = EH_MAX_TELEMETRY_QUEUE). */
  uint8_t telemetry_queue_capacity;
  /** 0 = fila só em RAM (sem malloc/`eh.tel.q`). 1 = persistir via store_*. */
  uint8_t telemetry_queue_persist;
} eh_config_t;

typedef struct {
  int (*http_post)(
    void *user_ctx,
    const char *url,
    const char *headers,
    const uint8_t *body,
    size_t body_len,
    uint8_t *resp,
    size_t resp_cap,
    size_t *resp_len,
    int *http_status
  );

  int (*store_read)(void *user_ctx, const char *key, uint8_t *buf, size_t cap, size_t *len);
  int (*store_write)(void *user_ctx, const char *key, const uint8_t *buf, size_t len);
  int (*telemetry_collect)(void *user_ctx, eh_values_writer_t *writer);
  /** Opcional: preenche keys do check-in (mesmo padrão eh_values_add_* da telemetria). */
  int (*checkin_keys_collect)(void *user_ctx, eh_values_writer_t *writer);
  int (*on_command)(void *user_ctx, const char *execution_id, const char *command_key, const char *payload_json);
  /**
   * Opcional: download oferecido no check-in (por asset key).
   * Invocado uma vez por entrada em `downloads` da resposta v2.
   */
  void (*on_checkin_download)(
    void *user_ctx,
    const char *asset_key,
    const char *version,
    const char *download_url,
    const char *checksum_sha256
  );
  /**
   * Opcional: configuração oferecida no check-in (por asset key).
   * `payload_json` é o objeto JSON serializado do campo `payload`.
   */
  void (*on_checkin_configuration)(
    void *user_ctx,
    const char *asset_key,
    const char *version,
    const char *source,
    const char *payload_json
  );
  /**
   * Opcional: preenche message_out antes do POST executed automático.
   * Retorna número de bytes escritos (sem NUL) ou 0 se vazio.
   */
  int (*on_command_message)(
    void *user_ctx,
    const char *execution_id,
    const char *command_key,
    int success,
    char *message_out,
    size_t message_cap
  );
  /** @deprecated Reservado ABI; check-in envia JSON plano. */
  int (*checkin_encrypt_json)(
    void *user_ctx,
    const uint8_t *plain_json,
    size_t plain_len,
    uint8_t *out_body,
    size_t out_cap,
    size_t *out_len
  );
  void (*log_fn)(void *user_ctx, eh_log_level_t level, const char *msg);
  void *user_ctx;
  /**
   * Opcional (extensão ABI 1, no fim da struct): igual a `http_post` com Retry-After.
   * Se não-NULL, a lib usa este em vez de `http_post` e honra `Retry-After`
   * em qualquer status HTTP (`Retry-After` em segundos → `eh_client_t.last_retry_after_sec`).
   * Firmware existente deixa NULL — a assinatura de `http_post` não muda.
   */
  int (*http_post_ex)(
    void *user_ctx,
    const char *url,
    const char *headers,
    const uint8_t *body,
    size_t body_len,
    uint8_t *resp,
    size_t resp_cap,
    size_t *resp_len,
    int *http_status,
    uint32_t *retry_after_sec
  );
} eh_callbacks_t;

typedef struct eh_client {
  eh_config_t cfg;
  eh_callbacks_t cb;
  uint8_t initialized;

  eh_provisioning_status_t provisioning;

  char secret_current[EH_MAX_SECRET];
  char secret_previous[EH_MAX_SECRET];
  uint8_t has_secret_current;
  uint8_t has_secret_previous;

  uint64_t next_checkin_at_ms;
  uint64_t next_telemetry_at_ms;
  uint64_t backoff_until_ms;
  uint32_t backoff_current_ms;

  eh_pending_command_t pending_commands[EH_MAX_COMMANDS];
  uint8_t pending_command_count;

  /** JSON object (ex. {"firmware":"1.0.0"}) incluído em check-in body e HMAC. */
  char checkin_keys_json[EH_MAX_CHECKIN_KEYS];
  /** Snapshot das keys do último check-in HTTP OK (comparação de drift). */
  char last_sent_checkin_keys_json[EH_MAX_CHECKIN_KEYS];
  uint8_t has_last_sent_checkin_keys;

  uint8_t identity_seed[32];
  uint8_t has_identity_seed;

  uint8_t needs_credential_reprovision;
  /** Segundos do último Retry-After (401/429/etc); consumido em schedule_backoff. */
  uint32_t last_retry_after_sec;

  eh_queued_reading_t telemetry_queue[EH_MAX_TELEMETRY_QUEUE];
  uint8_t telemetry_queue_head;
  uint8_t telemetry_queue_count;

  /** Unix ms de `credential.expiresAt`; 0 = sem expiração conhecida. Recompile header + lib juntos. */
  uint64_t credential_expires_at_ms;
} eh_client_t;

EH_API eh_status_t eh_client_init(eh_client_t *cli, const eh_config_t *cfg, const eh_callbacks_t *cb, uint64_t now_ms);
EH_API eh_status_t eh_client_tick(eh_client_t *cli, uint64_t now_ms);
EH_API eh_status_t eh_client_force_checkin(eh_client_t *cli, uint64_t now_ms);
EH_API eh_status_t eh_client_force_telemetry(eh_client_t *cli, uint64_t now_ms);
/** POST telemetria `{values:{}}` para puxar `pendingCommands` mesmo com `enable_telemetry=0`. */
EH_API eh_status_t eh_client_poll_downlink(eh_client_t *cli, uint64_t now_ms);

EH_API eh_status_t eh_client_set_group_product(eh_client_t *cli, const char *group_key, const char *product_code);
EH_API eh_status_t eh_client_set_secret(eh_client_t *cli, const char *current_secret, const char *previous_secret);
EH_API eh_status_t eh_client_set_telemetry_ts(eh_client_t *cli, const char *ts_iso);
/** Atualiza intervalo automático (ms). `0` usa default 10000. Ignorado quando `telemetry_manual_only`. */
EH_API eh_status_t eh_client_set_telemetry_interval(eh_client_t *cli, uint32_t interval_ms);
/** Máx. de leituras por POST (`0` = 1, corpo single legado). */
EH_API eh_status_t eh_client_set_telemetry_batch_max(eh_client_t *cli, uint8_t batch_max);
/** Recolhe `telemetry_collect` e enfileira (ts vazio → ISO-8601 de `now_ms`). */
EH_API eh_status_t eh_client_enqueue_reading(eh_client_t *cli, const char *ts_iso, uint64_t now_ms);
/** Envia até `telemetry_batch_max` leituras da fila. Em falha HTTP as leituras permanecem. */
EH_API eh_status_t eh_client_flush_telemetry(eh_client_t *cli, uint64_t now_ms);
EH_API uint8_t eh_client_telemetry_queue_count(const eh_client_t *cli);
/** Esvazia a fila RAM. Com `telemetry_queue_persist=0` não toca em storage. */
EH_API void eh_client_telemetry_queue_clear(eh_client_t *cli);
/** Atualiza intervalo periódico de check-in (ms). `0` usa default 60000. */
EH_API eh_status_t eh_client_set_checkin_interval(eh_client_t *cli, uint32_t interval_ms);
/** @param keys_json_object JSON object (`{...}`) ou NULL/"" para `{}`. Ignorado se `checkin_keys_collect` estiver definido. */
EH_API eh_status_t eh_client_set_checkin_keys_json(eh_client_t *cli, const char *keys_json_object);
/** Seed Ed25519 bruto (32 bytes) para credential/issue. */
EH_API eh_status_t eh_client_set_identity_seed(eh_client_t *cli, const uint8_t seed[32]);
/** PKCS#8 Ed25519 em base64/base64url (export MDM). */
EH_API eh_status_t eh_client_set_identity_private_key_pkcs8_b64(eh_client_t *cli, const char *pkcs8_b64);

EH_API eh_provisioning_status_t eh_client_get_provisioning(const eh_client_t *cli);
EH_API void eh_client_get_status(const eh_client_t *cli, eh_client_status_t *out);
EH_API eh_status_t eh_client_get_pending_command(
  const eh_client_t *cli,
  uint8_t index,
  eh_pending_command_t *out
);
EH_API eh_status_t eh_client_confirm_command_execution(
  eh_client_t *cli,
  const char *execution_id,
  int success,
  const char *message
);

EH_API size_t eh_struct_size_config(void);
EH_API size_t eh_struct_size_client(void);
EH_API size_t eh_struct_size_callbacks(void);
EH_API size_t eh_struct_size_status(void);

/** Returns "MAJOR.MINOR.PATCH" string baked into the linked library. */
EH_API const char *eh_lib_version_string(void);

/** Returns EH_LIB_ABI_VERSION from the linked library. */
EH_API unsigned eh_lib_abi_version(void);

/**
 * Compares caller-supplied struct sizes / ABI version with the linked library.
 * `eh_lib_abi_check()` passes the header's `sizeof` and `EH_LIB_ABI_VERSION`.
 */
EH_API eh_status_t eh_lib_abi_check_impl(
  size_t config_sz,
  size_t client_sz,
  size_t callbacks_sz,
  size_t status_sz,
  unsigned abi
);

/**
 * Verifies header vs linked library ABI (struct sizes + ABI version).
 * Returns EH_OK on match, EH_ERR_INVALID_ARG on mismatch.
 */
#ifndef EH_LIB_ABI_CHECK_EXPORT
static inline eh_status_t eh_lib_abi_check(void)
{
  return eh_lib_abi_check_impl(
    sizeof(eh_config_t),
    sizeof(eh_client_t),
    sizeof(eh_callbacks_t),
    sizeof(eh_client_status_t),
    EH_LIB_ABI_VERSION
  );
}
#else
EH_API eh_status_t eh_lib_abi_check(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EDGEHUB_DEVICE_H */
