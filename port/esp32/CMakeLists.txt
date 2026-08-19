get_filename_component(EHC_PORT_ESP32_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(EHC_SRC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

idf_component_register(
    SRCS
        "${EHC_SRC_ROOT}/src/ehc_core.c"
        "${EHC_SRC_ROOT}/src/ehc_telemetry.c"
        "${EHC_SRC_ROOT}/src/ehc_commands.c"
        "${EHC_SRC_ROOT}/src/ehc_checkin.c"
        "${EHC_SRC_ROOT}/src/ehc_json.c"
        "${EHC_SRC_ROOT}/src/ehc_bridge.c"
        "${EHC_SRC_ROOT}/src/ehc_identity.c"
        "${EHC_PORT_ESP32_DIR}/ehc_port_esp32.c"
    INCLUDE_DIRS
        "${EHC_SRC_ROOT}/include"
        "${EHC_PORT_ESP32_DIR}"
    PRIV_INCLUDE_DIRS
        "${EHC_SRC_ROOT}/src"
    REQUIRES
        edgehub_device
        esp_http_client
        esp-tls
        nvs_flash
        esp_timer
        esp_hw_support
        freertos
)
