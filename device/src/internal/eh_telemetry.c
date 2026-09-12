#include "eh_telemetry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eh_auth.h"
#include "eh_commands.h"
#include "eh_http_retry.h"
#include "eh_json.h"
#include "eh_log.h"
#include "eh_provision.h"
#include "eh_telemetry_queue.h"

static int eh_is_leap_year(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

/** ISO-8601 UTC from `now_ms` (Unix epoch). Empty on overflow. */
void eh_format_iso8601_utc(uint64_t now_ms, char *out, size_t cap) {
  static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint64_t sec;
  unsigned msec;
  unsigned days;
  unsigned rem;
  int year = 1970;
  int month;
  int dim;
  int n;

  if (!out || cap < 25) {
    if (out && cap) out[0] = '\0';
    return;
  }

  sec = now_ms / 1000ULL;
  msec = (unsigned)(now_ms % 1000ULL);
  days = (unsigned)(sec / 86400ULL);
  rem = (unsigned)(sec % 86400ULL);

  for (;;) {
    int diy = eh_is_leap_year(year) ? 366 : 365;
    if (days < (unsigned)diy) break;
    days -= (unsigned)diy;
    year++;
    if (year > 9999) {
      out[0] = '\0';
      return;
    }
  }

  for (month = 0; month < 12; month++) {
    dim = mdays[month];
    if (month == 1 && eh_is_leap_year(year)) dim = 29;
    if (days < (unsigned)dim) break;
    days -= (unsigned)dim;
  }

  n = snprintf(
    out,
    cap,
    "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ",
    year,
    month + 1,
    (int)days + 1,
    (int)(rem / 3600u),
    (int)((rem % 3600u) / 60u),
    (int)(rem % 60u),
    msec
  );
  if (n <= 0 || (size_t)n >= cap) out[0] = '\0';
}

static eh_status_t build_telemetry_url(eh_client_t *cli, char *url, size_t cap) {
  int n;
  if (cli->cfg.tenant_slug[0]) {
    n = snprintf(
      url, cap, "%s/api/v1/telemetry/public/%s/device/telemetry",
      cli->cfg.base_url, cli->cfg.tenant_slug
    );
  } else {
    n = snprintf(url, cap, "%s/api/v1/telemetry/public/device/telemetry", cli->cfg.base_url);
  }
  if (n <= 0 || (size_t)n >= cap) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static eh_status_t build_values_json(eh_client_t *cli, char *out, size_t cap) {
  eh_values_writer_t writer;
  eh_status_t rc = eh_values_begin(&writer, out, cap);
  if (rc != EH_OK) return rc;
  if (cli->cb.telemetry_collect && !cli->cb.telemetry_collect(cli->cb.user_ctx, &writer)) {
    return EH_ERR_IO;
  }
  return eh_values_end(&writer);
}

static eh_status_t build_telemetry_body(
  eh_client_t *cli,
  const char *ts,
  const char *values_json,
  char *out,
  size_t cap,
  size_t *out_len
) {
  size_t len = 0;

  if (!eh_json_append(out, cap, &len, "{\"deviceUid\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, cli->cfg.device_uid)) return EH_ERR_BUFFER_TOO_SMALL;

  if (ts && ts[0]) {
    if (!eh_json_append(out, cap, &len, "\",\"ts\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_escape_append(out, cap, &len, ts)) return EH_ERR_BUFFER_TOO_SMALL;
    if (!eh_json_append(out, cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  } else {
    if (!eh_json_append(out, cap, &len, "\"")) return EH_ERR_BUFFER_TOO_SMALL;
  }

  if (!eh_json_append(out, cap, &len, ",\"values\":")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, values_json)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "}")) return EH_ERR_BUFFER_TOO_SMALL;
  *out_len = len;
  return EH_OK;
}

static eh_status_t append_reading_object(
  char *out,
  size_t cap,
  size_t *len,
  const eh_queued_reading_t *reading
) {
  if (!eh_json_append(out, cap, len, "{\"ts\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, len, reading->ts)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, len, "\",\"values\":")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, len, reading->values_json[0] ? reading->values_json : "{}")) {
    return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!eh_json_append(out, cap, len, "}")) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static eh_status_t build_telemetry_batch_body(
  eh_client_t *cli,
  uint8_t count,
  char *out,
  size_t cap,
  size_t *out_len
) {
  size_t len = 0;
  uint8_t i;

  if (!eh_json_append(out, cap, &len, "{\"deviceUid\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_escape_append(out, cap, &len, cli->cfg.device_uid)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\",\"readings\":[")) return EH_ERR_BUFFER_TOO_SMALL;
  for (i = 0; i < count; i++) {
    const eh_queued_reading_t *reading = eh_telemetry_queue_peek(cli, i);
    if (!reading) return EH_ERR_INVALID_ARG;
    if (i > 0 && !eh_json_append(out, cap, &len, ",")) return EH_ERR_BUFFER_TOO_SMALL;
    if (append_reading_object(out, cap, &len, reading) != EH_OK) return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!eh_json_append(out, cap, &len, "]}")) return EH_ERR_BUFFER_TOO_SMALL;
  *out_len = len;
  return EH_OK;
}

static eh_status_t build_telemetry_batch_canonical(
  eh_client_t *cli,
  uint8_t count,
  char *out,
  size_t cap
) {
  size_t len = 0;
  uint8_t i;

  /* Paridade byte-a-byte com canonicalTelemetryBatchPayload (JSON.stringify, sem espaços). */
  if (!eh_json_append(out, cap, &len, "{\"deviceUid\":\"")) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, cli->cfg.device_uid)) return EH_ERR_BUFFER_TOO_SMALL;
  if (!eh_json_append(out, cap, &len, "\",\"alias\":\"\",\"readings\":[")) return EH_ERR_BUFFER_TOO_SMALL;
  for (i = 0; i < count; i++) {
    const eh_queued_reading_t *reading = eh_telemetry_queue_peek(cli, i);
    if (!reading) return EH_ERR_INVALID_ARG;
    if (i > 0 && !eh_json_append(out, cap, &len, ",")) return EH_ERR_BUFFER_TOO_SMALL;
    if (append_reading_object(out, cap, &len, reading) != EH_OK) return EH_ERR_BUFFER_TOO_SMALL;
  }
  if (!eh_json_append(out, cap, &len, "]}")) return EH_ERR_BUFFER_TOO_SMALL;
  return EH_OK;
}

static int parse_rejected_index(const char *json, int *out_index) {
  const char *p;
  char *end = NULL;
  long value;

  if (!json || !out_index) return 0;
  p = strstr(json, "\"rejectedIndex\"");
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  value = strtol(p, &end, 10);
  if (end == p || value < 0 || value > 255) return 0;
  *out_index = (int)value;
  return 1;
}

eh_status_t eh_telemetry_collect_and_enqueue(eh_client_t *cli, const char *ts_iso, uint64_t now_ms) {
  char values_json[EH_MAX_TELEMETRY_QUEUE_VALUES];
  char ts_auto[EH_MAX_TS];
  const char *ts;
  eh_status_t rc;

  if (!cli) return EH_ERR_INVALID_ARG;
  rc = build_values_json(cli, values_json, sizeof(values_json));
  if (rc != EH_OK) return rc;

  ts = ts_iso;
  if (!ts || !ts[0]) ts = cli->cfg.telemetry_ts;
  if (!ts[0]) {
    eh_format_iso8601_utc(now_ms, ts_auto, sizeof(ts_auto));
    ts = ts_auto;
  }
  return eh_telemetry_enqueue(cli, ts, values_json);
}

eh_status_t eh_flush_telemetry(eh_client_t *cli) {
  char url[EH_MAX_URL + 128];
  char *body = NULL;
  char *canonical = NULL;
  uint8_t resp[EH_MAX_RESPONSE];
  size_t body_len = 0;
  size_t resp_len = 0;
  int http_status = 0;
  uint32_t retry_after_sec = 0;
  eh_status_t rc;
  int http_ok;
  uint8_t batch_max;
  uint8_t n;
  const eh_queued_reading_t *first;
  int rejected_index = -1;

  if (!cli || (!cli->cb.http_post && !cli->cb.http_post_ex)) return EH_ERR_INVALID_ARG;
  if (!cli->cfg.enable_telemetry) return EH_OK;
  if (cli->telemetry_queue_count == 0) return EH_OK;

  batch_max = eh_telemetry_batch_max(cli);
  n = cli->telemetry_queue_count;
  if (n > batch_max) n = batch_max;

  rc = build_telemetry_url(cli, url, sizeof(url));
  if (rc != EH_OK) return rc;

  body = (char *)malloc(EH_MAX_BATCH_PAYLOAD);
  canonical = (char *)malloc(EH_MAX_BATCH_PAYLOAD);
  if (!body || !canonical) {
    free(body);
    free(canonical);
    return EH_ERR_IO;
  }
  body[0] = '\0';
  canonical[0] = '\0';

  if (batch_max <= 1) {
    first = eh_telemetry_queue_peek(cli, 0);
    if (!first) {
      rc = EH_ERR_INVALID_ARG;
      goto done;
    }
    rc = build_telemetry_body(cli, first->ts, first->values_json, body, EH_MAX_BATCH_PAYLOAD, &body_len);
    if (rc != EH_OK) goto done;
    snprintf(
      canonical, EH_MAX_BATCH_PAYLOAD,
      "{\"deviceUid\":\"%s\",\"alias\":\"\",\"ts\":\"%s\",\"values\":%s}",
      cli->cfg.device_uid,
      first->ts,
      first->values_json[0] ? first->values_json : "{}"
    );
  } else {
    rc = build_telemetry_batch_body(cli, n, body, EH_MAX_BATCH_PAYLOAD, &body_len);
    if (rc != EH_OK) goto done;
    rc = build_telemetry_batch_canonical(cli, n, canonical, EH_MAX_BATCH_PAYLOAD);
    if (rc != EH_OK) goto done;
  }

  if (!cli->has_secret_current) {
    rc = EH_ERR_AUTH;
    goto done;
  }

  http_ok = eh_client_http_post_hmac(
    cli, url, canonical, 1, (const uint8_t *)body, body_len,
    resp, sizeof(resp), &resp_len, &http_status, &retry_after_sec
  );
  if (resp_len >= sizeof(resp)) resp_len = sizeof(resp) - 1;
  resp[resp_len] = '\0';

  if (!http_ok || http_status < 200 || http_status > 299) {
    eh_client_note_http_retry(cli, http_status, retry_after_sec);
    if (eh_telemetry_response_requires_credential_reprovision(http_status, (const char *)resp)) {
      cli->needs_credential_reprovision = 1;
      eh_logf(cli, EH_LOG_WARN, "telemetry_credential_reprovision_required");
    }
    if (parse_rejected_index((const char *)resp, &rejected_index) &&
        rejected_index >= 0 && rejected_index < (int)n) {
      eh_telemetry_queue_drop_at(cli, (uint8_t)rejected_index);
      eh_logf(cli, EH_LOG_WARN, "telemetry_rejected_index_dropped");
    }
    rc = EH_ERR_HTTP;
    goto done;
  }

  eh_telemetry_queue_drop_front(cli, n);
  eh_parse_pending_commands(cli, (const char *)resp);
  eh_logf(cli, EH_LOG_INFO, "telemetry_ok");
  rc = EH_OK;

done:
  free(body);
  free(canonical);
  return rc;
}

eh_status_t eh_run_telemetry(eh_client_t *cli, uint64_t now_ms) {
  eh_status_t rc;
  if (!cli || (!cli->cb.http_post && !cli->cb.http_post_ex)) return EH_ERR_INVALID_ARG;
  if (!cli->cfg.enable_telemetry) return EH_OK;
  rc = eh_telemetry_collect_and_enqueue(cli, NULL, now_ms);
  if (rc != EH_OK) return rc;
  return eh_flush_telemetry(cli);
}

eh_status_t eh_poll_downlink(eh_client_t *cli, uint64_t now_ms) {
  char url[EH_MAX_URL + 128];
  char ts[EH_MAX_TS];
  char body[384];
  char canonical[512];
  uint8_t *resp = NULL;
  size_t body_len = 0;
  size_t resp_len = 0;
  int http_status = 0;
  uint32_t retry_after_sec = 0;
  eh_status_t rc = EH_ERR_IO;
  int http_ok;
  enum { EH_POLL_RESP = 2048 };

  if (!cli || (!cli->cb.http_post && !cli->cb.http_post_ex)) return EH_ERR_INVALID_ARG;
  if (!cli->has_secret_current) return EH_ERR_AUTH;

  resp = (uint8_t *)malloc(EH_POLL_RESP);
  if (!resp) {
    eh_logf(cli, EH_LOG_WARN, "downlink_poll_oom");
    return EH_ERR_IO;
  }

  rc = build_telemetry_url(cli, url, sizeof(url));
  if (rc != EH_OK) goto done;
  eh_format_iso8601_utc(now_ms, ts, sizeof(ts));
  rc = build_telemetry_body(cli, ts, "{}", body, sizeof(body), &body_len);
  if (rc != EH_OK) goto done;
  snprintf(
    canonical, sizeof(canonical),
    "{\"deviceUid\":\"%s\",\"alias\":\"\",\"ts\":\"%s\",\"values\":{}}",
    cli->cfg.device_uid,
    ts[0] ? ts : ""
  );

  {
    char poll_msg[56];
    char short_ts[25];
    size_t n = 0;
    while (ts[n] && n < sizeof(short_ts) - 1) {
      short_ts[n] = ts[n];
      n++;
    }
    short_ts[n] = '\0';
    snprintf(poll_msg, sizeof(poll_msg), "downlink_poll ts=%s", short_ts[0] ? short_ts : "?");
    eh_logf(cli, EH_LOG_INFO, poll_msg);
  }
  http_ok = eh_client_http_post_hmac(
    cli, url, canonical, 1, (const uint8_t *)body, body_len,
    resp, EH_POLL_RESP, &resp_len, &http_status, &retry_after_sec
  );
  if (resp_len >= EH_POLL_RESP) resp_len = EH_POLL_RESP - 1;
  resp[resp_len] = '\0';

  if (!http_ok || http_status < 200 || http_status > 299) {
    char fail_msg[48];
    eh_client_note_http_retry(cli, http_status, retry_after_sec);
    if (eh_telemetry_response_requires_credential_reprovision(http_status, (const char *)resp)) {
      cli->needs_credential_reprovision = 1;
      eh_logf(cli, EH_LOG_WARN, "telemetry_credential_reprovision_required");
    }
    snprintf(fail_msg, sizeof(fail_msg), "downlink_poll_fail http=%d", http_status);
    eh_logf(cli, EH_LOG_WARN, fail_msg);
    rc = EH_ERR_HTTP;
    goto done;
  }

  eh_parse_pending_commands(cli, (const char *)resp);
  eh_logf(cli, EH_LOG_INFO, "downlink_poll_ok");
  rc = EH_OK;

done:
  free(resp);
  return rc;
}
