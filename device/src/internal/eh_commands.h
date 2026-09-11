#ifndef EH_COMMANDS_H
#define EH_COMMANDS_H

#include "../../include/edgehub_device.h"

eh_status_t eh_process_pending_commands(eh_client_t *cli, uint64_t now_ms);
eh_status_t eh_parse_pending_commands(eh_client_t *cli, const char *json);

#endif /* EH_COMMANDS_H */
