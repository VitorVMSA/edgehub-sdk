#include "eh_provision.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eh_auth.h"
#include "eh_http_retry.h"
#include "eh_identity.h"
#include "eh_json.h"
#include "eh_log.h"

static eh_status_t build_credential_issue_url(eh_client_t *cli, char *url, size_t cap) {
  int n;

  if (cli->cfg.tenant_slug[0]) {
    n = snprintf(
      url, cap, "%s/api/v1/telemetry/public/%s/device/credential/issue",
      cli->cfg.base_url, cli->cfg.tenant_slug
    );
  } else {
    n = snprintf(url, cap, "%s/api/v1/telemetry/public/device/credential/issue", cli->cfg.base_url);
  }
  if (n <= 0 || (size_t)n >= cap) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static eh_status_t build_canonical_identity_challenge_payload(
  const char *challenge_id,
  const char *device_uid,
  const char *nonce,
  char *out,
  size_t cap,
  size_t *out_len
) {
  size_t len = 0;

  if (!out || !out_len || !challenge_id || !device_uid || !nonce) return EH_ERR_INVALID_ARG;
  out[0] = '\0';

  if (!eh_json_append(out, cap, &len, "{\"challengeId\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, challenge_id)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\",\"deviceUid\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, device_uid)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\",\"nonce\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, nonce)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\"}")) return EH_ERR_BUFFER_TOO_SMALL;
  *out_len = len;
  return EH_OK;
}

static int eh_is_leap_year_p(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

/** ISO-8601 UTC (`...Z`). 0 ms → 1 para não colidir com «sem expiração». */
static int parse_iso8601_utc_ms(const char *iso, uint64_t *out_ms) {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;
  int msec = 0;
  int n;
  int y;
  int m;
  unsigned days = 0;
  static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint64_t total_sec;

  if (!iso || !out_ms) return 0;
  n = sscanf(iso, "%d-%d-%dT%d:%d:%d.%dZ", &year, &month, &day, &hour, &min, &sec, &msec);
  if (n != 7) {
    n = sscanf(iso, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &min, &sec);
    if (n != 6) return 0;
    msec = 0;
  }
  if (year < 1970 || year > 9999) return 0;
  if (month < 1 || month > 12) return 0;
  if (day < 1 || day > 31) return 0;
  if (hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 60) return 0;
  if (msec < 0 || msec > 999) return 0;

  for (y = 1970; y < year; y++) {
    days += eh_is_leap_year_p(y) ? 366u : 365u;
  }
  for (m = 1; m < month; m++) {
    int dim = mdays[m - 1];
    if (m == 2 && eh_is_leap_year_p(year)) dim = 29;
    days += (unsigned)dim;
  }
  days += (unsigned)(day - 1);
  total_sec = (uint64_t)days * 86400ULL + (uint64_t)hour * 3600ULL + (uint64_t)min * 60ULL + (uint64_t)sec;
  *out_ms = total_sec * 1000ULL + (uint64_t)msec;
  if (*out_ms == 0) *out_ms = 1;
  return 1;
}

static eh_status_t apply_secret_from_issue_response(eh_client_t *cli, const char *resp_json) {
  char credential_obj[512];
  char secret[EH_MAX_SECRET];
  char rotate_grace[64];
  char expires_at[64];

  credential_obj[0] = '\0';
  secret[0] = '\0';
  rotate_grace[0] = '\0';
  expires_at[0] = '\0';
  if (!eh_find_json_raw_value(resp_json, "credential", credential_obj, sizeof(credential_obj))) {
    return EH_ERR_PARSE;
  }
  if (!eh_find_json_string_value(credential_obj, "secret", secret, sizeof(secret)) || !secret[0]) {
    return EH_ERR_PARSE;
  }

  if (cli->has_secret_current) {
    eh_client_set_secret(cli, secret, cli->secret_current);
  } else {
    eh_client_set_secret(cli, secret, "");
  }
  cli->needs_credential_reprovision = 0;
  cli->credential_expires_at_ms = 0;
  if (eh_find_json_string_value(credential_obj, "rotateGraceUntil", rotate_grace, sizeof(rotate_grace)) && rotate_grace[0]) {
    eh_logf(cli, EH_LOG_INFO, "identity_credential_rotate_grace");
  }
  if (eh_find_json_string_value(credential_obj, "expiresAt", expires_at, sizeof(expires_at)) && expires_at[0]) {
    uint64_t expires_ms = 0;
    eh_logf(cli, EH_LOG_INFO, "identity_credential_expires_at");
    if (parse_iso8601_utc_ms(expires_at, &expires_ms)) {
      cli->credential_expires_at_ms = expires_ms;
    }
  }
  eh_logf(cli, EH_LOG_INFO, "identity_credential_issued");
  return EH_OK;
}

static eh_status_t exchange_identity_challenge(
  eh_client_t *cli,
  const char *challenge_id,
  const char *nonce
) {
  char url[EH_MAX_URL + 160];
  char headers[EH_MAX_HEADERS];
  char signature_b64[EH_MAX_ED25519_SIG_B64];
  char *canonical = NULL;
  char *body = NULL;
  uint8_t *resp = NULL;
  size_t canonical_len = 0;
  size_t body_len = 0;
  size_t resp_len = 0;
  int http_status = 0;
  uint32_t retry_after_sec = 0;
  eh_status_t rc = EH_ERR_IO;
  int n;

  if (!cli || !challenge_id || !challenge_id[0] || !nonce || !nonce[0]) return EH_ERR_INVALID_ARG;
  if (!cli->cb.http_post && !cli->cb.http_post_ex) return EH_ERR_INVALID_ARG;

  enum { EH_ISSUE_CANONICAL = 512, EH_ISSUE_BODY = 768, EH_ISSUE_RESP = 1536 };

  if (!cli->has_identity_seed) {
    eh_logf(cli, EH_LOG_WARN, "identity_seed_missing");
    return EH_ERR_AUTH;
  }

  canonical = (char *)malloc(EH_ISSUE_CANONICAL);
  body = (char *)malloc(EH_ISSUE_BODY);
  resp = (uint8_t *)malloc(EH_ISSUE_RESP);
  if (!canonical || !body || !resp) {
    eh_logf(cli, EH_LOG_WARN, "identity_challenge_oom");
    rc = EH_ERR_IO;
    goto done;
  }

  rc = build_canonical_identity_challenge_payload(
    challenge_id, cli->cfg.device_uid, nonce, canonical, EH_ISSUE_CANONICAL, &canonical_len
  );
  if (rc != EH_OK) goto done;

  signature_b64[0] = '\0';
  rc = eh_ed25519_sign_utf8(cli, canonical, signature_b64, sizeof(signature_b64));
  if (rc != EH_OK) goto done;

  rc = build_credential_issue_url(cli, url, sizeof(url));
  if (rc != EH_OK) goto done;

  n = snprintf(
    body,
    EH_ISSUE_BODY,
    "{\"deviceUid\":\"%s\",\"challengeId\":\"%s\",\"signature\":\"%s\"}",
    cli->cfg.device_uid,
    challenge_id,
    signature_b64
  );
  if (n <= 0 || (size_t)n >= EH_ISSUE_BODY) {
    rc = EH_ERR_BUFFER_TOO_SMALL;
    goto done;
  }
  body_len = (size_t)n;

  snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");
  if (!eh_client_http_post(
      cli, url, headers, (const uint8_t *)body, body_len,
      resp, EH_ISSUE_RESP, &resp_len, &http_status, &retry_after_sec)) {
    eh_client_note_http_retry(cli, http_status, retry_after_sec);
    eh_logf(cli, EH_LOG_WARN, "credential_issue_transport_fail");
    rc = EH_ERR_HTTP;
    goto done;
  }
  if (http_status < 200 || http_status > 299) {
    char fail_msg[48];
    eh_client_note_http_retry(cli, http_status, retry_after_sec);
    snprintf(fail_msg, sizeof(fail_msg), "credential_issue_fail http=%d", http_status);
    eh_logf(cli, EH_LOG_WARN, fail_msg);
    rc = EH_ERR_HTTP;
    goto done;
  }
  if (resp_len >= EH_ISSUE_RESP) resp_len = EH_ISSUE_RESP - 1;
  resp[resp_len] = '\0';
  rc = apply_secret_from_issue_response(cli, (const char *)resp);

done:
  free(canonical);
  free(body);
  free(resp);
  return rc;
}

eh_status_t eh_apply_identity_credential_issue_from_checkin(eh_client_t *cli, const char *checkin_response_json) {
  char challenge_obj[384];
  char challenge_id[96];
  char nonce[128];

  if (!cli || !checkin_response_json) return EH_ERR_INVALID_ARG;
  if (!cli->cfg.enable_identity_credential_issue) return EH_OK;

  challenge_obj[0] = '\0';
  challenge_id[0] = '\0';
  nonce[0] = '\0';
  if (!eh_find_json_raw_value(checkin_response_json, "identityChallenge", challenge_obj, sizeof(challenge_obj))) {
    return EH_OK;
  }
  if (!eh_find_json_string_value(challenge_obj, "challengeId", challenge_id, sizeof(challenge_id)) || !challenge_id[0]) {
    return EH_OK;
  }
  if (!eh_find_json_string_value(challenge_obj, "nonce", nonce, sizeof(nonce)) || !nonce[0]) {
    return EH_OK;
  }

  eh_logf(cli, EH_LOG_INFO, "identity_challenge_received");
  return exchange_identity_challenge(cli, challenge_id, nonce);
}

int eh_telemetry_response_requires_credential_reprovision(int http_status, const char *response_json) {
  if (http_status != 401 || !response_json) return 0;
  if (eh_json_has_token(response_json, "TELEMETRY_DEVICE_CREDENTIAL_EXPIRED")) return 1;
  if (eh_json_has_token(response_json, "TELEMETRY_DEVICE_CREDENTIAL_INVALID")) return 1;
  if (eh_json_has_token(response_json, "TELEMETRY_DEVICE_CREDENTIAL_REVOKED")) return 1;
  if (eh_json_has_token(response_json, "TELEMETRY_DEVICE_CREDENTIAL_REQUIRED")) return 1;
  if (eh_json_has_token(response_json, "TELEMETRY_DEVICE_SIGNATURE_REQUIRED")) return 1;
  return 0;
}

int eh_checkin_response_requires_credential_reprovision(int http_status, const char *response_json) {
  if (http_status != 401 || !response_json) return 0;
  if (eh_json_has_token(response_json, "CHECKIN_DEVICE_CREDENTIAL_EXPIRED")) return 1;
  if (eh_json_has_token(response_json, "CHECKIN_DEVICE_CREDENTIAL_INVALID")) return 1;
  if (eh_json_has_token(response_json, "CHECKIN_DEVICE_CREDENTIAL_REVOKED")) return 1;
  if (eh_json_has_token(response_json, "CHECKIN_DEVICE_CREDENTIAL_REQUIRED")) return 1;
  if (eh_json_has_token(response_json, "CHECKIN_DEVICE_SIGNATURE_REQUIRED")) return 1;
  return 0;
}
