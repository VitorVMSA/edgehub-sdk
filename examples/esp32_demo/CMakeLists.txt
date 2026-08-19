cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EHC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
if(EXISTS "${EHC_ROOT}/third_party/edgehub-device-sdk/examples/esp32_minimal/components/edgehub_device")
  set(EH_DEVICE_COMPONENT "${EHC_ROOT}/third_party/edgehub-device-sdk/examples/esp32_minimal/components")
else()
  set(EH_DEVICE_COMPONENT "${EHC_ROOT}/../edgehub-device-sdk/examples/esp32_minimal/components")
endif()

set(EXTRA_COMPONENT_DIRS
  "${EH_DEVICE_COMPONENT}"
)

project(ehc_esp32_demo)
