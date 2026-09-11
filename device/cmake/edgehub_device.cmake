# Helper for ESP-IDF projects consuming a prebuilt edgehub_device library.
#
# Usage:
#   include(${EDGEHUB_SDK_ROOT}/cmake/edgehub_device.cmake)
#   edgehub_device_use_prebuilt(${EDGEHUB_SDK_ROOT}/lib/esp32s3/libedgehub_device.a
#                             ${EDGEHUB_SDK_ROOT}/include)

function(edgehub_device_use_prebuilt prebuilt_lib include_dir)
  if(NOT EXISTS "${prebuilt_lib}")
    message(FATAL_ERROR "edgehub_device prebuilt not found: ${prebuilt_lib}")
  endif()
  if(NOT EXISTS "${include_dir}/edgehub_device.h")
    message(FATAL_ERROR "edgehub_device.h not found in: ${include_dir}")
  endif()
  add_prebuilt_library(edgehub_device "${prebuilt_lib}")
  target_include_directories(edgehub_device INTERFACE "${include_dir}")
endfunction()
