# Runbook ESP32

Port: [PORTING.md](PORTING.md). Relógio: `now_ms` = epoch Unix. Heap: device 1.2.6.

## Sintoma: ingest 401 `TELEMETRY_INVALID_TS`, HMAC «válido»

**Causa:** `ts` ISO gerado de `now_ms` ≈ 1970 (`esp_timer` / gettimeofday ainda 0).

**Fix:** `now_ms` = `gettimeofday` se `tv_sec > 0`, senão fallback `esp_timer_get_time()/1000`; `TZ=UTC0` + `tzset`; 1.º POST lê `Date` se `tv_sec < 1600000000`; SNTP no firmware **antes** do loop de ingest. `ehc_tick` re-lê o relógio antes do poll.

**Não:** recusar epoch pequeno na lib; meter SNTP no port; tratar como secret inválido.

## Sintoma: 401 sem body / `http_status=0` / check-in nunca parseia challenge

**Causa:** `esp_http_client_perform` descarta 401 de aplicação (sem `WWW-Authenticate`). Ou RST keep-alive (errno 104) reutilizou status 202 antigo.

**Fix:** POST `open/write/fetch_headers/read`. `http_post` nonzero = transporte OK. Sem keep-alive. Um retry só em falha de **transporte**. 401 com JSON = sucesso de transporte.

**Não:** tratar 4xx como falha de transporte.

## Sintoma: HMAC/issue falha no WROOM com SSH ligado; stack overflow

**Causa:** buffers check-in (~12 KB) vivos + issue ~16 KB na mesma task MDM (~24 KB stack, sem PSRAM). Poll com `uint8_t resp[EH_MAX_RESPONSE]` (8192) na stack.

**Fix (já em 1.2.6):** stash challenge 512 B, free 12 KB, **depois** issue (malloc 512/768/1536). Poll malloc 2048.

**Não:** sobrepor `EH_MAX_PAYLOAD`/`EH_MAX_RESPONSE` na stack MDM; deixar check-in vivo durante issue.

## Sintoma: NVS corrompe / Guru Meditation no storage

**Causa:** I/O NVS em task com stack PSRAM.

**Fix:** `-DEHC_PORT_ESP32_STORAGE_WORKER=1` (worker 3072 words, queue 4) ou correr o client em RAM interna.

## Sintoma: após OTA, device `pending` e re-download de toda a config

**Causa:** port genérico `ehc`/`k%08x` em vez do mapa NVS que o produto já usava para HMAC/assets.

**Fix:** glue firmware `storage_read/write`. **Não** usar o NVS do port greenfield em campo já provisionado. Ver [PORTING.md](PORTING.md).

## Sintoma: `g_ehc` não cabe / WROOM 4 MB

**Não** definir `EHC_CTX_ATTR`. Reduzir `EH_MAX_TELEMETRY_QUEUE`. Slave: QUEUE=1, `disable_telemetry=1`.

## Diagnóstico do port

`ehc_port_esp32_last_http_status`, `last_http_op` (`checkin`\|`telemetry`\|`executed`\|`issue`) e `last_cmd` são úteis em teste. `ehc_port_esp32_last_http_snip` contém texto arbitrário do body e **não deve ser impresso, compartilhado ou anexado a logs**; pode ecoar telemetria ou dados sensíveis. Prefira status HTTP e eventos categorizados do SDK.

On-device: o primeiro POST após RST deve recuperar.

## Logs úteis

`checkin http=200`, `batch staged=8 sent=8 http=202`, `downlink_poll ts=`, `pending_cmd=`, `checkin_401_without_identity_challenge`.
