#ifndef EH_CHECKIN_H
#define EH_CHECKIN_H

#include "../../include/edgehub_device.h"

eh_status_t eh_run_checkin(eh_client_t *cli);
eh_status_t eh_checkin_build_keys_json(eh_client_t *cli, char *out, size_t cap);
void eh_checkin_store_sent_keys(eh_client_t *cli, const char *keys_json);
/** @return 1 se keys actuais != último envio, 0 se iguais ou sem snapshot, -1 em erro. */
int eh_checkin_keys_drifted(const eh_client_t *cli);

#endif /* EH_CHECKIN_H */
