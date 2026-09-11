#ifndef EH_TELEMETRY_H
#define EH_TELEMETRY_H

#include "../../include/edgehub_device.h"

void eh_format_iso8601_utc(uint64_t now_ms, char *out, size_t cap);
eh_status_t eh_telemetry_collect_and_enqueue(eh_client_t *cli, const char *ts_iso, uint64_t now_ms);
eh_status_t eh_flush_telemetry(eh_client_t *cli);
eh_status_t eh_run_telemetry(eh_client_t *cli, uint64_t now_ms);
/** POST `{deviceUid,ts,values:{}}` (ignora `enable_telemetry`). */
eh_status_t eh_poll_downlink(eh_client_t *cli, uint64_t now_ms);

#endif /* EH_TELEMETRY_H */
