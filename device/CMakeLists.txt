cmake_minimum_required(VERSION 3.16)
project(edgehub_device_sdk C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(EH_BUILD_HOST "Build host static library" ON)

set(EH_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(EH_BUILD_HOST)
  set(EH_PLATFORM host)
endif()
include(${EH_ROOT}/cmake/edgehub_sources.cmake)

if(EH_BUILD_HOST)
  add_library(edgehub_device STATIC ${EH_EDGEHUB_SOURCES})
  target_include_directories(edgehub_device
    PUBLIC ${EH_ROOT}/include
    PRIVATE ${EH_ROOT}/src/internal/ed25519_donna
  )
  target_compile_definitions(edgehub_device PRIVATE ED25519_REFHASH ED25519_CUSTOMRANDOM EH_EXPORTS)
  if(MSVC)
    target_compile_options(edgehub_device PRIVATE /W4)
  else()
    target_compile_options(edgehub_device PRIVATE -Wall -Wextra -Wno-implicit-fallthrough)
  endif()

  add_library(edgehub_device_host ALIAS edgehub_device)

  install(TARGETS edgehub_device ARCHIVE DESTINATION lib)
  install(FILES include/edgehub_device.h DESTINATION include)
  install(FILES cmake/edgehub_device.cmake DESTINATION cmake)

  option(EH_BUILD_TESTS "Build Unity unit tests" ON)
  if(EH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()
