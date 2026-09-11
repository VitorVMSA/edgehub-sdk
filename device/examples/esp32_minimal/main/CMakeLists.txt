idf_component_register(SRCS "dummy_main.c" "ed25519_random_esp32.c" INCLUDE_DIRS "." REQUIRES edgehub_device)

target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_SOURCE_DIR}/../../src/internal/ed25519_donna")
