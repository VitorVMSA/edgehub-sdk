#include "eh_telemetry_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eh_log.h"

static const char k_queue_store_key[] = "eh.tel.q";

uint8_t eh_telemetry_batch_max(const eh_client_t *cli) {
  uint8_t n;
  if (!cli) return EH_DEFAULT_TELEMETRY_BATCH_MAX;
  n = cli->cfg.telemetry_batch_max;
  if (n == 0) return EH_DEFAULT_TELEMETRY_BATCH_MAX;
  if (n > EH_MAX_TELEMETRY_QUEUE) return EH_MAX_TELEMETRY_QUEUE;
  return n;
}

uint8_t eh_telemetry_queue_capacity(const eh_client_t *cli) {
  uint8_t n;
  if (!cli) return EH_MAX_TELEMETRY_QUEUE;
  n = cli->cfg.telemetry_queue_capacity;
  if (n == 0 || n > EH_MAX_TELEMETRY_QUEUE) return EH_MAX_TELEMETRY_QUEUE;
  return n;
}

void eh_telemetry_queue_store(eh_client_t *cli) {
  size_t cap;
  uint8_t *blob;
  size_t off = 1;
  uint8_t i;

  if (!cli || !cli->cfg.telemetry_queue_persist || !cli->cb.store_write) return;
  cap = 1 + (size_t)cli->telemetry_queue_count * sizeof(eh_queued_reading_t);
  blob = (uint8_t *)malloc(cap);
  if (!blob) return;
  blob[0] = cli->telemetry_queue_count;
  for (i = 0; i < cli->telemetry_queue_count; i++) {
    const eh_queued_reading_t *slot = eh_telemetry_queue_peek(cli, i);
    if (!slot) break;
    memcpy(blob + off, slot, sizeof(*slot));
    off += sizeof(*slot);
  }
  (void)cli->cb.store_write(cli->cb.user_ctx, k_queue_store_key, blob, off);
  free(blob);
}

void eh_telemetry_queue_load(eh_client_t *cli) {
  size_t cap = 1 + (size_t)EH_MAX_TELEMETRY_QUEUE * sizeof(eh_queued_reading_t);
  uint8_t *blob;
  size_t len = 0;
  uint8_t count;
  uint8_t i;
  size_t off = 1;

  if (!cli || !cli->cfg.telemetry_queue_persist || !cli->cb.store_read) return;
  blob = (uint8_t *)malloc(cap);
  if (!blob) return;
  if (!cli->cb.store_read(cli->cb.user_ctx, k_queue_store_key, blob, cap, &len) || len < 1) {
    free(blob);
    return;
  }
  count = blob[0];
  if (count > EH_MAX_TELEMETRY_QUEUE) count = EH_MAX_TELEMETRY_QUEUE;
  if (len < 1u + (size_t)count * sizeof(eh_queued_reading_t)) {
    free(blob);
    return;
  }
  cli->telemetry_queue_head = 0;
  cli->telemetry_queue_count = 0;
  for (i = 0; i < count; i++) {
    memcpy(&cli->telemetry_queue[i], blob + off, sizeof(eh_queued_reading_t));
    cli->telemetry_queue[i].ts[EH_MAX_TS - 1] = '\0';
    cli->telemetry_queue[i].values_json[EH_MAX_TELEMETRY_QUEUE_VALUES - 1] = '\0';
    off += sizeof(eh_queued_reading_t);
    cli->telemetry_queue_count++;
  }
  free(blob);
}

const eh_queued_reading_t *eh_telemetry_queue_peek(const eh_client_t *cli, uint8_t offset) {
  uint8_t idx;
  if (!cli || offset >= cli->telemetry_queue_count) return NULL;
  idx = (uint8_t)((cli->telemetry_queue_head + offset) % EH_MAX_TELEMETRY_QUEUE);
  return &cli->telemetry_queue[idx];
}

void eh_telemetry_queue_drop_front(eh_client_t *cli, uint8_t n) {
  if (!cli || n == 0) return;
  if (n > cli->telemetry_queue_count) n = cli->telemetry_queue_count;
  cli->telemetry_queue_head = (uint8_t)((cli->telemetry_queue_head + n) % EH_MAX_TELEMETRY_QUEUE);
  cli->telemetry_queue_count = (uint8_t)(cli->telemetry_queue_count - n);
  eh_telemetry_queue_store(cli);
}

void eh_telemetry_queue_drop_at(eh_client_t *cli, uint8_t offset) {
  uint8_t i;
  if (!cli || offset >= cli->telemetry_queue_count) return;
  for (i = offset; i + 1 < cli->telemetry_queue_count; i++) {
    eh_queued_reading_t *dst = (eh_queued_reading_t *)eh_telemetry_queue_peek(cli, i);
    const eh_queued_reading_t *src = eh_telemetry_queue_peek(cli, (uint8_t)(i + 1));
    if (!dst || !src) break;
    memcpy(dst, src, sizeof(*dst));
  }
  if (cli->telemetry_queue_count > 0) {
    cli->telemetry_queue_count--;
  }
  eh_telemetry_queue_store(cli);
}

eh_status_t eh_telemetry_enqueue(eh_client_t *cli, const char *ts, const char *values_json) {
  eh_queued_reading_t *slot;
  uint8_t idx;

  if (!cli || !values_json) return EH_ERR_INVALID_ARG;
  if (cli->telemetry_queue_count >= eh_telemetry_queue_capacity(cli)) {
    eh_logf(cli, EH_LOG_WARN, "telemetry_queue_full_drop_oldest");
    eh_telemetry_queue_drop_front(cli, 1);
  }
  idx = (uint8_t)((cli->telemetry_queue_head + cli->telemetry_queue_count) % EH_MAX_TELEMETRY_QUEUE);
  slot = &cli->telemetry_queue[idx];
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->ts, sizeof(slot->ts), "%s", ts ? ts : "");
  snprintf(slot->values_json, sizeof(slot->values_json), "%s", values_json);
  cli->telemetry_queue_count++;
  eh_telemetry_queue_store(cli);
  return EH_OK;
}
