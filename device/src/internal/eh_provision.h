#ifndef EH_PROVISION_H
#define EH_PROVISION_H

#include "../../include/edgehub_device.h"

eh_status_t eh_apply_identity_credential_issue_from_checkin(eh_client_t *cli, const char *checkin_response_json);
int eh_telemetry_response_requires_credential_reprovision(int http_status, const char *response_json);
int eh_checkin_response_requires_credential_reprovision(int http_status, const char *response_json);

#endif /* EH_PROVISION_H */
