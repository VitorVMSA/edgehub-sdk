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
