cmake_minimum_required(VERSION 3.16)
project(edgehub_client_sdk C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(EHC_BUILD_TESTS "Build Unity host tests" ON)
option(EHC_TEST_DEVICE_SDK "Also build/run edgehub-device-sdk unit tests" OFF)
option(EHC_COVERAGE "Enable gcov/lcov coverage (GCC/Clang)" OFF)
option(EHC_BUILD_LINUX_DEMO "Build examples/linux_demo" ON)
option(EHC_DEVICE_SDK_PREBUILT "Link a prebuilt libedgehub_device instead of compiling sources" OFF)

set(EHC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT EHC_DEVICE_SDK_ROOT)
  if(EXISTS "${EHC_ROOT}/third_party/edgehub-device-sdk/CMakeLists.txt")
    set(EHC_DEVICE_SDK_ROOT "${EHC_ROOT}/third_party/edgehub-device-sdk")
  elseif(EXISTS "${EHC_ROOT}/../edgehub-device-sdk/CMakeLists.txt")
    set(EHC_DEVICE_SDK_ROOT "${EHC_ROOT}/../edgehub-device-sdk")
  endif()
endif()

if(NOT EHC_DEVICE_SDK_PREBUILT)
  if(NOT EHC_DEVICE_SDK_ROOT OR NOT EXISTS "${EHC_DEVICE_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
      "edgehub-device-sdk not found. Set EHC_DEVICE_SDK_ROOT or run scripts/sync_device_sdk.ps1 "
      "(expected third_party/edgehub-device-sdk or ../edgehub-device-sdk, tag >= v1.2.3).")
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

if(UNIX)
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
