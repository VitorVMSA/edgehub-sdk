#ifndef EH_TELEMETRY_QUEUE_H
#define EH_TELEMETRY_QUEUE_H

#include "../../include/edgehub_device.h"

uint8_t eh_telemetry_batch_max(const eh_client_t *cli);
uint8_t eh_telemetry_queue_capacity(const eh_client_t *cli);

eh_status_t eh_telemetry_enqueue(eh_client_t *cli, const char *ts, const char *values_json);
const eh_queued_reading_t *eh_telemetry_queue_peek(const eh_client_t *cli, uint8_t offset);
void eh_telemetry_queue_drop_front(eh_client_t *cli, uint8_t n);
void eh_telemetry_queue_drop_at(eh_client_t *cli, uint8_t offset);
void eh_telemetry_queue_load(eh_client_t *cli);
void eh_telemetry_queue_store(eh_client_t *cli);

#endif /* EH_TELEMETRY_QUEUE_H */
