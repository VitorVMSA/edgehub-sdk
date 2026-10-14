# EdgeHub Device SDK

Biblioteca C (caixa preta) para MDM/Telemetry EdgeHub. Versão **1.2.6**, ABI **2**. Vive in-tree em `edgehub-client-sdk/device/`.

**Docs:** [`docs/README.md`](../docs/README.md) · [`docs/PROTOCOL.md`](../docs/PROTOCOL.md) · [`device/docs/INTEGRATION.md`](docs/INTEGRATION.md).

## Integração mínima

1. Alocar `eh_client_t` e preencher `eh_config_t`.
2. Registar `eh_callbacks_t` (mínimo: `http_post` ou `http_post_ex`).
3. `eh_lib_abi_check()` no boot.
4. Loop: `eh_client_tick(&cli, now_ms)` com `now_ms` = epoch Unix.
5. Telemetria: `eh_values_begin` / `eh_values_add_*` / `eh_values_end`.

Firmwares EcoCircuito usam a camada `ehc_*` do client, não este header sozinho.

## Testes (host, a partir da raiz do umbrella)

```powershell
.\scripts\test_host.ps1 -DeviceSdk
```

Payloads ~4 KiB: compilar **fontes** com `-DEH_MAX_TELEMETRY_QUEUE=8` e `-DEH_MAX_TELEMETRY_QUEUE_VALUES=4096`. O `.a` default 768 não serve. `EH_MAX_BATCH_PAYLOAD` permanece 64 KiB.

## Ed25519 RNG

Com `ED25519_CUSTOMRANDOM`, o integrador fornece `ed25519_randombytes_unsafe`. Host: `platform/ed25519_random_host.c`. MCU: `port/embedded/ed25519_random_embedded.c` (TRNG). Não ligar os dois.

Header e objectos da **mesma** versão (`eh_lib_abi_check()`). Parser JSON: `eh_json.c` (sem `vendor/cjson`).
