cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EHC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(EH_DEVICE_COMPONENT "${EHC_ROOT}/device/examples/esp32_minimal/components")

set(EXTRA_COMPONENT_DIRS
  "${EH_DEVICE_COMPONENT}"
)

project(ehc_esp32_demo)
