# Porting

O HAL está em `include/edgehub_client/ehc_port.h`. Detalhe de arquitectura: [ARCHITECTURE.md](ARCHITECTURE.md). Runbook: [RUNBOOK_ESP32.md](RUNBOOK_ESP32.md).

Implemente:

| Callback | Notas |
|---|---|
| `http_post` | **nonzero = transporte OK**, mesmo em HTTP 4xx/5xx. Preencha `http_status` e `retry_after_sec` quando o servidor enviar `Retry-After` (não só em 429). Use esta variante (`http_post_ex` no device SDK). **Não há `http_get` no HAL:** o GET de `downloadUrl` (OTA) fica em `on_download` na aplicação. |
| `storage_read` / `storage_write` | blobs opacos. Segredo HMAC em `ehc.hmac.secret` / `.prev`. Versões de asset em `ehc.asset.<key>`. **Não** mapear `eh.tel.q` — a fila do client é só RAM. |
| `now_ms` | milissegundos. **Ingest** (`ts` ISO-8601 auto-preenchido, HMAC, `expiresAt`) exige epoch Unix. Relógio sintético/monotónico só é válido em testes host. ESP32: `gettimeofday`; SNTP é firmware, não desta lib. |
| `log` | opcional |
| `lock` / `unlock` | opcional; obrigatório se `ehc_tick` e HTTP/storage puderem correr em threads distintas. `ehc_lock` é reentrante (`lock_depth`): `ehc_tel_set_*` e `ehc_tel_reset` são seguros entre tasks e a partir de callbacks da lib (`on_config_apply`, `on_download`). |

Troca de alvo: o client e `device/` ficam iguais. CMake host: `-DEHC_PORT=host|linux|embedded|esp32` (`esp32` = projecto IDF `examples/esp32_demo`).

## Linux (`port/linux`)

OpenSSL BIO com `SO_RCVTIMEO`/`SO_SNDTIMEO` (default 12 s). Storage = um ficheiro por chave num directório. `clock_gettime(CLOCK_REALTIME)`. Mutex pthread. `ehc_thread_start(period_ms)` arranca um loop de `ehc_tick`.

## Embarcado genérico (`port/embedded`)

C99 sem POSIX/IDF. Stubs: `http_post` devolve 0 (transporte falhou); storage RAM **8×192 B**; `now_ms` sintético; lock NULL. Demo: `examples/embedded_stub` (`ehc_embedded_stub`).

No MCU: copiar `port/embedded/ehc_port_embedded.c` e substituir HTTPS, flash e mutex. Ligar `ed25519_random_embedded.c` **em vez de** `device/platform/ed25519_random_host.c` (`rand()`) e trocar o **LCG** por TRNG. Não compilar os dois no mesmo binário.

## ESP32 (`port/esp32`) — greenfield

`esp_http_client` + `esp_crt_bundle_attach`. POST via `open`/`write`/`fetch_headers`/`read` (não `perform`) para o body de HTTP 401 de aplicação chegar ao SDK — o EdgeHub usa 401 sem `WWW-Authenticate`. Lê `Retry-After` no event de header. Sem keep-alive. **Uma** repetição se o transporte falhar (RST); HTTP 4xx/5xx não dispara retry. Contexto de resposta na stack. NVS namespace `ehc` (chaves mapeadas para 15 caracteres via FNV-1a `k%08x`).

`g_ehc` em PSRAM: `-DEHC_USER_CONFIG_HEADER` com `#define EHC_CTX_ATTR __attribute__((section(".ext_ram.bss")))` — não editar `ehc_core.c`. Sem PSRAM (WROOM 4 MB): não definir `EHC_CTX_ATTR`.

`now_ms` (`esp_now`): `gettimeofday` se `tv_sec > 0`; senão fallback `esp_timer_get_time()/1000`. No init: `TZ=UTC0` + `tzset`. Se o relógio ainda for < 2020 (`tv_sec < 1600000000`), o primeiro POST lê o cabeçalho HTTP `Date` (RFC1123) e chama `settimeofday`. **SNTP (`pool.ntp.org`) é do firmware**, não do port. Sem epoch Unix o ingest assina `ts` 1970 → HTTP 401 `TELEMETRY_INVALID_TS` com HMAC válido.

Diagnóstico: `ehc_port_esp32_last_http_status` / `last_http_op` (`checkin` | `telemetry` | `executed` | `issue`) / `last_cmd` (último `pending_cmd=`). `ehc_port_esp32_last_http_snip` expõe parte arbitrária do body; não a imprima nem compartilhe em logs.

On-device: o primeiro POST após RST deve recuperar.

**Regra dura:** I/O de NVS só em tasks com stack em **RAM interna**. Se a task do client corre em PSRAM, compile com `-DEHC_PORT_ESP32_STORAGE_WORKER=1` — worker com stack 3072 words, queue 4.

Compile as **fontes** de `device/` **1.2.6** (já sem `vendor/cjson` nos `INCLUDE_DIRS`). O `.a` prebuilt com `EH_MAX_TELEMETRY_QUEUE_VALUES=768` **não** serve para payloads maiores — use `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096` (e `-DEH_MAX_TELEMETRY_QUEUE=8`) no mesmo compile que o header. `EH_MAX_BATCH_PAYLOAD` permanece 64 KiB. No slave, `EH_MAX_TELEMETRY_QUEUE=1` e `disable_telemetry=1`. Check-in/issue/poll no device usam `malloc` pequeno (não buffers de 8–12 KB na stack da task MDM).

RNG: `examples/esp32_demo/main/ed25519_random_esp32.c` (`esp_random`). CMake device: `ED25519_CUSTOMRANDOM` (+ `ED25519_REFHASH`).

## Storage legado (EcoCircuito já provisionado)

**Não** use o NVS genérico `ehc` / `k%08x` num equipamento que já tem HMAC e versões de asset. Sem tradução, o segredo desaparece, o check-in volta a `pending` e as versões caem em `"0"` (re-download de toda a configuração MDM).

O conteúdo é string plana nos dois lados. O glue do firmware implementa `storage_read` / `storage_write`:

| Chave `ehc_*` | Destino legado |
|---|---|
| `ehc.hmac.secret` | `config_manager` / NVS `config_manager` / `edgehub_hmac_secret` |
| `ehc.hmac.secret.prev` | `edgehub_hmac_secret_prev` |
| `ehc.asset.<key>` | namespace `EdgeHubAssetVer`, chaves `eh_v_*` via `edgehub_asset_version_get/set` |

Identidade (`base_url`, UID, group, product, PKCS#8) → `EHC_IDENT_FN` sobre o `config_manager`, **não** storage do client.

Glue NVS legado **não** precisa de mapear `eh.tel.q`. O client não persiste a fila de telemetria.

I/O NVS continua nas tasks de RAM interna que já existem.

## Host stub (`port/host_stub`)

Servidor HTTP falso, relógio `ehc_test_clock_advance`, storage em memória **32 chaves**, falha injectável. Só para testes. Default `EHC_PORT=host`.

## ABI

`ehc_init` chama `eh_lib_abi_check()` primeiro. Mismatch → `EHC_EABI`.
