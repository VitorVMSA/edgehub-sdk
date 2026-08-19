cmake_minimum_required(VERSION 3.16)

add_library(ehc_unity STATIC vendor/unity/unity.c)
target_include_directories(ehc_unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity)

function(ehc_add_test name)
  add_executable(${name} ${name}.c)
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EHC_ROOT}/include
    ${EHC_ROOT}/port/host_stub
    ${EHC_ROOT}/src
  )
  target_link_libraries(${name} PRIVATE edgehub_client ehc_port_host ehc_unity)
  if(MSVC)
    target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  if(EHC_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${name} PRIVATE --coverage -O0 -g)
    target_link_options(${name} PRIVATE --coverage)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

ehc_add_test(test_readiness)
ehc_add_test(test_checkin)
ehc_add_test(test_telemetry)
ehc_add_test(test_telemetry_batch)
ehc_add_test(test_commands)
ehc_add_test(test_assets)
ehc_add_test(test_offline)

if(EHC_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_custom_target(ehc_coverage
    COMMAND ${CMAKE_COMMAND} -E echo "Run tests then gcov on src/*.c (piso sugerido: 70%)"
  )
endif()
