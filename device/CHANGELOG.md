# Changelog

## 1.2.4 — 2026-09-11

- Check-in 401 `CHECKIN_DEVICE_CREDENTIAL_REVOKED` e `CHECKIN_DEVICE_CREDENTIAL_REQUIRED` armam `needs_credential_reprovision` (paridade com telemetria).
- HMAC: um retry com `secret_previous` em 401 `*_CREDENTIAL_INVALID` (check-in, telemetria, executed). `credential.expiresAt` parseado; no tick arma reprovision. `eh_client_t` cresceu (`credential_expires_at_ms`) — **ABI 2**, recompile header + lib juntos.
- `eh_client_poll_downlink`: POST telemetria `{values:{}}` mesmo com `enable_telemetry=0` para puxar `pendingCommands`.
- Check-in offer: download sem `checksumSha256` (string vazia); config sem `source` (default `""`). 6.º `pendingCommands` loga `pending_commands_truncated`.
- `eh_client_tick` não aborta a telemetria do mesmo tick quando o check-in falha.
- `pendingCommands` com `commandKey` nulo ou ausente são enfileirados (`command_key` vazio) e seguem `on_command` / auto-confirm.
- `eh_lib_abi_check()` passa a ser inline no header e chama `eh_lib_abi_check_impl` com os `sizeof` do TU do firmware — detecta header ≠ `.a`.
- `provisioningStatus` lido via parser de string (aceita espaço após `:`).
- Body de check-in faz JSON-escape de `deviceUid` / `groupKey` / `productCode`.
- **ABI 2 unchanged.**

## 1.2.3 — 2026-09-11

- Check-in: processa `identityChallenge` também em HTTP 4xx (ex. 401). Se o `credential/issue` emitir HMAC, o check-in trata-se como sucesso.
- HTTP 401 `CHECKIN_DEVICE_IDENTITY_NOT_REGISTERED`: backoff 300 s quando o port não envia `Retry-After`.
- `eh_client_note_http_retry` honra `Retry-After` em qualquer status (não só 429).
- Parser JSON: strings aceitam espaço após `:` (`"challengeId": "…"`).
- Logs: `checkin_oom`, `checkin_fail http=`, `checkin_response_truncated`.

## 1.2.2 — 2026-08-19

- `eh_client_telemetry_queue_clear()` empties the in-RAM ring.
- `eh_config_t.telemetry_queue_persist`: `0` skips `eh.tel.q` store/load (no malloc of the blob). Default in tests/examples that want offline persist is `1`. `eh_config_t` grew by one byte — **ABI 2**, recompile header and `.a` together.

## 1.2.1 — 2026-08-19

- Stop compiling unused `vendor/cjson` (`cJSON.c` is not referenced from `src/`; parser is `eh_json.c`). ESP32 component no longer exports `vendor/cjson` in `INCLUDE_DIRS` — avoids colliding with a firmware `cJson` component when building SDK sources.
- **ABI 2 unchanged.** Default `EH_MAX_TELEMETRY_QUEUE_VALUES` remains **768**. Large payloads (e.g. EcoCircuito ~4 KiB) must compile sources with `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096` (suggested `-DEH_MAX_TELEMETRY_QUEUE=8`, ~32 KiB vs ~12 KiB). Prebuilt `.a` with the default 768 **cannot** carry those payloads. `vendor/cjson/` stays in the tree unused.

## 1.2.0 — 2026-08-19

- `eh_values_add_raw(w, key, raw_json)`: insert a nested JSON value (object/array/number) without wrapping it as a string. Reuses `add_kv_prefix` + `eh_json_append`. **ABI 2 unchanged** (no struct size change).
- Unity unit tests under `tests/` covering the values writer, HMAC golden vectors, check-in, telemetry batch/queue, commands, scheduling and ABI.

## 1.1.0 — 2026-08-18

- Telemetry batch ingest: `telemetry_batch_max` > 1 sends `readings[]` (HMAC `canonicalTelemetryBatchPayload`). Default `0`/`1` keeps the legacy single `ts`/`values` body.
- Offline queue: `eh_client_enqueue_reading` / `eh_client_flush_telemetry` / `eh_client_telemetry_queue_count`. Failed HTTP keeps readings; `401` with `rejectedIndex` drops only that slot.
- Optional persist via existing `store_read` / `store_write` (`eh.tel.q`). `telemetry_queue_capacity` caps the ring (compile max `EH_MAX_TELEMETRY_QUEUE`).
- Body/canonical HMAC buffers moved to heap (`EH_MAX_BATCH_PAYLOAD`).
- **ABI 2**: `eh_config_t` / `eh_client_t` grew — recompile firmware; `eh_lib_abi_check()` rejects mixed header/.a.

## 1.0.3 — 2026-08-13

- Telemetry ingest now fills `ts` with ISO-8601 UTC from `now_ms` when `telemetry_ts` is empty (body + HMAC canonical). `eh_client_set_telemetry_ts` still wins when set.
- HTTP 429 `Retry-After`: optional `http_post_ex` (end of `eh_callbacks_t`) + `last_retry_after_sec`. **`http_post` signature unchanged (ABI 1).** Struct sizes grew — recompile firmware with this header; `eh_lib_abi_check()` rejects mixed header/.a.

## 1.0.2 — 2026-07-25

- Built-in MDM command `device.force_checkin`: handled inside the lib (calls `eh_client_force_checkin`, does **not** invoke `on_command`).
- Confirm via `command_executed` when `enable_auto_confirm_commands` is set.
- Host simulator coverage for force check-in command path.

## 1.0.1 — 2026-07-25

- Larger check-in entry/payload/response buffers (heap-backed) for configuration assets.

## 1.0.0 — 2026-07-06

- Initial release as prebuilt SDK (caixa preta).
- Extracted from EcoCircuito `components/edgehub_device` (branch `feature/edgehub-integration`).
- Public API: `edgehub_device.h` with `eh_client_*`, `eh_values_*`, callbacks.
- ABI helpers: `EH_LIB_VERSION_*`, `eh_lib_abi_check()`.
- Host (`libedgehub_device_host.a`) and ESP32-S3 (`libedgehub_device.a`) build targets.
- Platform hook: `ed25519_randombytes_unsafe` must be provided by integrator on ESP32 (see `examples/`).
