set(EH_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

set(EH_EDGEHUB_SOURCES
  ${EH_LIB_DIR}/src/edgehub_device.c
  ${EH_LIB_DIR}/src/internal/eh_auth.c
  ${EH_LIB_DIR}/src/internal/eh_b64.c
  ${EH_LIB_DIR}/src/internal/eh_checkin.c
  ${EH_LIB_DIR}/src/internal/eh_command_executed.c
  ${EH_LIB_DIR}/src/internal/eh_commands.c
  ${EH_LIB_DIR}/src/internal/eh_hmac.c
  ${EH_LIB_DIR}/src/internal/eh_identity.c
  ${EH_LIB_DIR}/src/internal/eh_json.c
  ${EH_LIB_DIR}/src/internal/eh_log.c
  ${EH_LIB_DIR}/src/internal/eh_provision.c
  ${EH_LIB_DIR}/src/internal/eh_sha256.c
  ${EH_LIB_DIR}/src/internal/eh_telemetry.c
  ${EH_LIB_DIR}/src/internal/eh_telemetry_queue.c
  ${EH_LIB_DIR}/src/internal/ed25519_donna/ed25519.c
)

if(CMAKE_HOST_BUILD OR (DEFINED EH_PLATFORM AND EH_PLATFORM STREQUAL "host"))
  list(APPEND EH_EDGEHUB_SOURCES ${EH_LIB_DIR}/platform/ed25519_random_host.c)
endif()
