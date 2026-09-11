#include "eh_identity.h"

#include <string.h>

#include "../../include/edgehub_device.h"
#include "eh_b64.h"
#include "ed25519.h"

/* ed25519_randombytes_unsafe: provided by platform glue when ED25519_CUSTOMRANDOM is set. */

static eh_status_t extract_ed25519_seed_from_pkcs8(const uint8_t *der, size_t der_len, uint8_t seed[32]) {
  size_t i;

  if (!der || der_len < 34) return EH_ERR_PARSE;
  for (i = 0; i + 34 <= der_len; ++i) {
    if (der[i] == 0x04 && der[i + 1] == 0x20) {
      memcpy(seed, der + i + 2, 32);
      return EH_OK;
    }
  }
  return EH_ERR_PARSE;
}

EH_API eh_status_t eh_client_set_identity_seed(eh_client_t *cli, const uint8_t seed[32]) {
  if (!cli || !seed) return EH_ERR_INVALID_ARG;
  memcpy(cli->identity_seed, seed, 32);
  cli->has_identity_seed = 1;
  return EH_OK;
}

EH_API eh_status_t eh_client_set_identity_private_key_pkcs8_b64(eh_client_t *cli, const char *pkcs8_b64) {
  uint8_t der[256];
  uint8_t seed[32];
  size_t der_len = 0;
  eh_status_t rc;

  if (!cli || !pkcs8_b64 || !pkcs8_b64[0]) return EH_ERR_INVALID_ARG;
  rc = eh_b64_decode(pkcs8_b64, der, sizeof(der), &der_len);
  if (rc != EH_OK) return rc;
  rc = extract_ed25519_seed_from_pkcs8(der, der_len, seed);
  if (rc != EH_OK) return rc;
  return eh_client_set_identity_seed(cli, seed);
}

eh_status_t eh_ed25519_sign_utf8(const eh_client_t *cli, const char *message_utf8, char *out_b64url, size_t out_cap) {
  ed25519_secret_key sk;
  ed25519_public_key pk;
  ed25519_signature sig;
  size_t out_len = 0;
  eh_status_t rc;

  if (!cli || !message_utf8 || !out_b64url || out_cap < 2) return EH_ERR_INVALID_ARG;
  if (!cli->has_identity_seed) return EH_ERR_AUTH;

  memcpy(sk, cli->identity_seed, 32);
  ed25519_publickey(sk, pk);
  ed25519_sign((const unsigned char *)message_utf8, strlen(message_utf8), sk, pk, sig);

  rc = eh_b64url_encode(sig, sizeof(sig), out_b64url, out_cap, &out_len);
  if (rc != EH_OK) return rc;
  if (out_len >= out_cap) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}
