# Testes

Harness: Unity vendored em `tests/vendor/unity/` + CTest. Um executável por módulo.

```powershell
scripts/test_host.ps1 -DeviceSdk
# ou
cmake -S . -B build -DEHC_BUILD_TESTS=ON -DEHC_TEST_DEVICE_SDK=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Em Unix: `scripts/test_host.sh`.

## O que está coberto (client)

| Módulo | Comportamento |
|---|---|
| `test_readiness` | sem URL/UID não há HTTP; `EHC_KEY_REQUIRED`; log throttled; MINIMAL→FULL |
| `test_checkin` | keys const/fn/asset no corpo |
| `test_telemetry` | JSON aninhado, omitir objecto vazio, getter vs push, profundidade, pool de strings, `%.15g` |
| `test_telemetry_batch` | `enqueue`/`flush`/`queued`; lote `readings[]`; tick não drena em `telemetry_manual_only`; `publish` imediato; `reset` omite STR condicional; `queue_clear` após 401; delta `queued`; `eh.tel.q` não chega ao port |
| `test_commands` | args tipados, desconhecido, `DEFER_AFTER_ACK`, rejeitar `device.force_checkin` |
| `test_assets` | versão só após apply OK; falha de storage; `on_download` no check-in |
| `test_offline` | fila até ACTIVE; 401 → FULL (reprovision) |
| `test_features` | `disable_telemetry=1`: zero POST `/device/telemetry`; `poll_commands=1` no slave faz POST `values:{}` e dispatch; PKCS#8 liga identity issue; wrappers runtime; os três `disable_*` → `EHC_ECONFIG` |

Cada teste passa o seu `ehc_user_t` (as tabelas **não** estão na `.a`).

## Device SDK

Com `EHC_TEST_DEVICE_SDK=ON` (default) o `ctest` inclui os testes Unity de `device/` com prefixo `eh_` (`eh_test_values_writer`, HMAC dourado, check-in, telemetria, comandos, scheduling, ABI) para não colidir com os nomes do client.

## Cobertura

`-DEHC_COVERAGE=ON` (GCC/Clang) acrescenta `--coverage`. Piso sugerido: 70 % das linhas de `src/`. No MSVC a opção é ignorada — os testes host correm na mesma.
