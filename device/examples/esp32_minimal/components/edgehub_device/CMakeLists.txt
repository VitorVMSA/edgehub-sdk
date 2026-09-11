cmake_minimum_required(VERSION 3.16)

set(EH_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include(${EH_SDK_ROOT}/cmake/edgehub_sources.cmake)

idf_component_register(
    SRCS
        ${EH_EDGEHUB_SOURCES}
    INCLUDE_DIRS
        "${EH_SDK_ROOT}/include"
    PRIV_INCLUDE_DIRS
        "${EH_SDK_ROOT}/src/internal/ed25519_donna"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE ED25519_REFHASH ED25519_CUSTOMRANDOM)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-implicit-fallthrough)
