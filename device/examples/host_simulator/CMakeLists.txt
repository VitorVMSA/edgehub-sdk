cmake_minimum_required(VERSION 3.16)
project(edgehub_host_simulator C)

set(EH_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
if(NOT EH_PLATFORM)
  set(EH_PLATFORM host)
endif()
include(${EH_SDK_ROOT}/cmake/edgehub_sources.cmake)
add_library(edgehub_device STATIC ${EH_EDGEHUB_SOURCES})
target_include_directories(edgehub_device
    PUBLIC ${EH_SDK_ROOT}/include
    PRIVATE ${EH_SDK_ROOT}/src/internal/ed25519_donna
)
target_compile_definitions(edgehub_device PRIVATE ED25519_REFHASH ED25519_CUSTOMRANDOM EH_EXPORTS)
if(MSVC)
  target_compile_options(edgehub_device PRIVATE /W4)
else()
  target_compile_options(edgehub_device PRIVATE -Wno-implicit-fallthrough)
endif()

add_executable(host_simulator main.c)
target_link_libraries(host_simulator PRIVATE edgehub_device)

enable_testing()
add_test(NAME abi_and_tick COMMAND host_simulator)
