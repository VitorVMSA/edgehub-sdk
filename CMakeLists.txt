cmake_minimum_required(VERSION 3.16)
project(edgehub_client_sdk C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(EHC_BUILD_TESTS "Build Unity host tests" ON)
option(EHC_TEST_DEVICE_SDK "Also build/run in-tree device SDK unit tests" ON)
option(EHC_COVERAGE "Enable gcov/lcov coverage (GCC/Clang)" OFF)
option(EHC_BUILD_LINUX_DEMO "Build examples/linux_demo" ON)
option(EHC_BUILD_EMBEDDED_STUB "Build port/embedded + examples/embedded_stub" ON)
option(EHC_DEVICE_SDK_PREBUILT "Link a prebuilt libedgehub_device instead of compiling sources" OFF)
set(EHC_PORT "host" CACHE STRING "Port to select: host (tests), linux, embedded, esp32 (IDF)")
set_property(CACHE EHC_PORT PROPERTY STRINGS host linux embedded esp32)

if(EHC_PORT STREQUAL "esp32")
  message(STATUS "EHC_PORT=esp32: use IDF project examples/esp32_demo (not this host CMake).")
endif()
if(EHC_PORT STREQUAL "linux" AND NOT UNIX)
  message(WARNING "EHC_PORT=linux requires Unix + OpenSSL; skipping ehc_port_linux.")
endif()

set(EHC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT EHC_DEVICE_SDK_ROOT)
  set(EHC_DEVICE_SDK_ROOT "${EHC_ROOT}/device")
endif()

if(NOT EHC_DEVICE_SDK_PREBUILT)
  if(NOT EXISTS "${EHC_DEVICE_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
      "edgehub-device-sdk not found at EHC_DEVICE_SDK_ROOT=${EHC_DEVICE_SDK_ROOT} "
      "(expected ${EHC_ROOT}/device, ABI 2 / 1.2.4).")
  endif()
  if(NOT EHC_TEST_DEVICE_SDK)
    set(EH_BUILD_TESTS OFF CACHE BOOL "Build device SDK Unity tests")
  else()
    set(EH_BUILD_TESTS ON CACHE BOOL "Build device SDK Unity tests")
  endif()
  set(EH_BUILD_HOST ON CACHE BOOL "Build device SDK host library")
  add_subdirectory("${EHC_DEVICE_SDK_ROOT}" "${CMAKE_BINARY_DIR}/edgehub-device-sdk")
  if(TARGET edgehub_device)
    target_compile_definitions(edgehub_device PRIVATE EH_LIB_ABI_CHECK_EXPORT)
  endif()
  if(MSVC AND TARGET edgehub_device)
    target_compile_definitions(edgehub_device PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endif()

set(EHC_SOURCES
  src/ehc_core.c
  src/ehc_telemetry.c
  src/ehc_commands.c
  src/ehc_checkin.c
  src/ehc_json.c
  src/ehc_bridge.c
  src/ehc_identity.c
)

add_library(edgehub_client STATIC ${EHC_SOURCES})
target_include_directories(edgehub_client
  PUBLIC
    ${EHC_ROOT}/include
  PRIVATE
    ${EHC_ROOT}/src
)
if(EHC_DEVICE_SDK_PREBUILT)
  if(NOT EHC_DEVICE_SDK_LIB OR NOT EHC_DEVICE_SDK_INCLUDE)
    message(FATAL_ERROR "EHC_DEVICE_SDK_PREBUILT requires EHC_DEVICE_SDK_LIB and EHC_DEVICE_SDK_INCLUDE")
  endif()
  target_include_directories(edgehub_client PUBLIC ${EHC_DEVICE_SDK_INCLUDE})
  target_link_libraries(edgehub_client PUBLIC ${EHC_DEVICE_SDK_LIB})
else()
  target_link_libraries(edgehub_client PUBLIC edgehub_device)
endif()

if(MSVC)
  target_compile_definitions(edgehub_client PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(edgehub_client PRIVATE /W4)
else()
  target_compile_options(edgehub_client PRIVATE -Wall -Wextra)
endif()

if(EHC_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(edgehub_client PRIVATE --coverage -O0 -g)
  target_link_options(edgehub_client PRIVATE --coverage)
endif()

add_library(ehc_port_host STATIC port/host_stub/ehc_port_host.c)
target_include_directories(ehc_port_host PUBLIC ${EHC_ROOT}/port/host_stub ${EHC_ROOT}/include)
target_link_libraries(ehc_port_host PUBLIC edgehub_client)
if(MSVC)
  target_compile_definitions(ehc_port_host PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(EHC_BUILD_EMBEDDED_STUB OR EHC_PORT STREQUAL "embedded")
  add_library(ehc_port_embedded STATIC port/embedded/ehc_port_embedded.c)
  target_include_directories(ehc_port_embedded PUBLIC ${EHC_ROOT}/port/embedded ${EHC_ROOT}/include)
  target_link_libraries(ehc_port_embedded PUBLIC edgehub_client)
  if(MSVC)
    target_compile_definitions(ehc_port_embedded PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(ehc_port_embedded PRIVATE -Wall -Wextra)
  endif()
  add_executable(ehc_embedded_stub
    examples/embedded_stub/main.c
    user_template/ehc_user_telemetry.c
    user_template/ehc_user_commands.c
    user_template/ehc_user_checkin.c
  )
  target_include_directories(ehc_embedded_stub PRIVATE
    ${EHC_ROOT}/user_template
    ${EHC_ROOT}/port/embedded
  )
  target_link_libraries(ehc_embedded_stub PRIVATE ehc_port_embedded)
  if(MSVC)
    target_compile_definitions(ehc_embedded_stub PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endif()

if(UNIX AND (EHC_PORT STREQUAL "host" OR EHC_PORT STREQUAL "linux"))
  find_package(OpenSSL)
  if(OpenSSL_FOUND)
    add_library(ehc_port_linux STATIC port/linux/ehc_port_linux.c)
    target_include_directories(ehc_port_linux PUBLIC ${EHC_ROOT}/port/linux ${EHC_ROOT}/include)
    target_link_libraries(ehc_port_linux PUBLIC edgehub_client OpenSSL::SSL OpenSSL::Crypto pthread)
    target_compile_options(ehc_port_linux PRIVATE -Wall -Wextra)
  else()
    message(STATUS "OpenSSL not found — skipping ehc_port_linux")
  endif()
endif()

if(EHC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(EHC_BUILD_LINUX_DEMO AND UNIX AND TARGET ehc_port_linux)
  add_executable(ehc_linux_demo
    examples/linux_demo/main.c
    user_template/ehc_user_telemetry.c
    user_template/ehc_user_commands.c
    user_template/ehc_user_checkin.c
  )
  target_include_directories(ehc_linux_demo PRIVATE
    ${EHC_ROOT}/user_template
    ${EHC_ROOT}/examples/linux_demo
    ${EHC_ROOT}/port/linux
  )
  target_link_libraries(ehc_linux_demo PRIVATE ehc_port_linux)
endif()
