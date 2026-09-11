#ifndef EH_COMMAND_EXECUTED_H
#define EH_COMMAND_EXECUTED_H

#include "../../include/edgehub_device.h"

eh_status_t eh_run_command_executed(
  eh_client_t *cli,
  const char *execution_id,
  int success,
  const char *message
);

#endif /* EH_COMMAND_EXECUTED_H */
