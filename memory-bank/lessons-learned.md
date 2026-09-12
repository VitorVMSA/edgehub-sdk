# Lessons Learned

> Formato: `YYYY-MM | área | lição`

---

### 2026-09 | CMake | Nomes CTest device vs client

**Causa:** `add_subdirectory(device)` com testes Unity `test_checkin` / `test_telemetry` / `test_commands` colidia com os executáveis do client.

**Fix:** `device/tests/CMakeLists.txt` prefixa alvos e testes `eh_*`.

**Evitar:** `add_test(NAME test_checkin)` em dois subprojectos no mesmo CMake.

---

### 2026-09 | API | Off memset-safe

**Causa:** `enable_telemetry=0` em `ehc_user_t` faria `memset(&user,0)` desligar o master EcoCircuito.

**Fix:** `disable_telemetry` / `disable_commands` / `disable_checkin`. Zero = tudo ligado.

**Evitar:** flags `enable_*` no user do client.

---

### 2026-09 | Device ABI | Três enable a 0

**Causa:** `eh_client_init` se `enable_checkin/telemetry/commands` forem todos 0, volta a ligar os três.

**Fix:** `ehc_init` recusa os três `disable_*` a 1 (`EHC_ECONFIG`). Device ABI 2 inalterado. Slave só `disable_telemetry=1`.

**Evitar:** `disable_checkin=disable_telemetry=disable_commands=1`.

---

### 2026-09 | ESP32 | HTTP 401 sem WWW-Authenticate

**Causa:** `esp_http_client_perform` descarta body de 401 de aplicação.

**Fix:** POST via `open`/`write`/`fetch_headers`/`read`. `http_post` nonzero = transporte OK.

**Evitar:** tratar 4xx como falha de transporte.

---

### 2026-09 | Import | Sem commit no subtree

**Causa:** `git subtree add` cria commit; AGENTS.md não permite commit sem pedido.

**Fix:** copiar `device/` excluindo `build/` e `dist/`.

**Evitar:** subtree/commit automático em entregas sem pedido de git.

---

### 2026-09 | Comandos | Downlink só na telemetria

**Causa:** EdgeHub não tem GET público de comandos; `pendingCommands` vem na resposta 2xx do POST telemetria. `executed` já existia.

**Fix:** documentado; opt-in `poll_commands` faz POST `{values:{}}` no mesmo ingest (conta `telemetry_events`).

**Evitar:** inventar URL de poll; ligar `enable_telemetry=1` por baixo no slave.

---

### 2026-09 | HMAC | Secret previous

**Causa:** o servidor aceita o secret antigo até `rotateGraceUntil`; a lib só assinava `secret_current`.

**Fix:** um retry HMAC com previous em 401 `*_CREDENTIAL_INVALID`. Harness/host stub **consomem** a reply matching (senão o 2.º POST via o mesmo 401). `expiresAt` vs `now_ms` só é útil com wall-clock Unix; ESP32 monotónico cai no 401 EXPIRED.

**Evitar:** retry em EXPIRED/REVOKED; reutilizar a mesma reply HTTP nos testes de retry; `schedule_backoff` no poll de comandos (atrasaria o check-in do slave).

---

### 2026-09 | HMAC | Números ≠ JSON.stringify

**Causa:** o ingest assina `JSON.stringify` do body parseado (`canonicalTelemetryBatchPayload`, com `alias:""`). `%.15g` em `|x|<1e-4` emite `1e-05`/`1e-005`; o Node emite `0.00001`. O HMAC do lote inteiro falha (401 `TELEMETRY_DEVICE_CREDENTIAL_INVALID`) mesmo com o segredo válido — o check-in (chaves string) continua 200. `%.17g` sozinho gera dígitos a mais que também falham o HMAC.

**Fix:** `eh_json_print_number` (shortest round-trip + regras ES `-6<n≤21`) em `eh_values_add_num`. Testes `eh_test_values_writer` + `eh_test_hmac_golden` com `1e-5`.

**Evitar:** patch só no glue EcoCircuito; alterar o contrato do servidor; usar `%.15g`/`%.17g` no body canónico.

---

### 2026-09 | ESP32 | `now_ms` e `TELEMETRY_INVALID_TS`

**Causa:** o ingest preenche `ts` ISO-8601 a partir de `now_ms`. O port ESP32 usava `esp_timer` (uptime). HMAC do body 1970 estava correcto; o servidor rejeitava `Invalid ts value`. Um segundo 1970 no mesmo `ehc_tick`: snapshot de `now` **antes** do check-in, poll **depois** do HTTP `Date`.

**Fix:** `gettimeofday` no port; `TZ=UTC0`; bootstrap HTTP `Date` se relógio < 2020. SNTP no firmware (não nesta lib). `ehc_tick` re-lê `ehc_now()` imediatamente antes do poll. A lib **não** recusa `now_ms` < 2020 (testes host usam relógio sintético).

**Evitar:** `esp_timer` como `now_ms` de ingest; recusar epoch pequeno em `eh_poll_downlink`; pôr SNTP dentro da `.a`.

---

### 2026-09 | ESP32 | Heap check-in + `credential/issue`

**Causa:** buffers de check-in (~12 KB) ainda vivos quando o issue alocava ~16 KB na mesma task MDM (~24 KB stack, WROOM sem PSRAM). HMAC/issue falhava com SSH ligado.

**Fix:** stash de `identityChallenge` em wrap 512 B, `free` dos 12 KB, **depois** issue. Issue: malloc 512/768/1536. Poll: malloc 2048 em vez de `uint8_t resp[EH_MAX_RESPONSE]` (8192) na stack.

**Evitar:** sobrepor `EH_MAX_PAYLOAD`/`EH_MAX_RESPONSE` na stack da task MDM; deixar buffers de check-in vivos durante o issue.
