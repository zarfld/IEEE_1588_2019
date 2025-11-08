# Phase 06 Integration Tests CMake
cmake_minimum_required(VERSION 3.16)

add_executable(boundary_clock_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_boundary_clock_integration.cpp
)
target_include_directories(boundary_clock_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(boundary_clock_integration PRIVATE IEEE1588_2019)

add_test(NAME boundary_clock_integration COMMAND boundary_clock_integration)
set_tests_properties(boundary_clock_integration PROPERTIES LABELS "integration")

add_executable(health_aggregation_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_health_aggregation_integration.cpp
)
target_include_directories(health_aggregation_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(health_aggregation_integration PRIVATE IEEE1588_2019)
add_test(NAME health_aggregation_integration COMMAND health_aggregation_integration)
set_tests_properties(health_aggregation_integration PROPERTIES LABELS "integration")

# Reliability harness (Phase 06)
add_executable(reliability_harness
    ${CMAKE_CURRENT_LIST_DIR}/reliability/reliability_harness.cpp
)
target_include_directories(reliability_harness PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(reliability_harness PRIVATE IEEE1588_2019)

add_test(NAME reliability_harness
    COMMAND reliability_harness 200 ${CMAKE_CURRENT_BINARY_DIR}/srg_failures.csv)
set_tests_properties(reliability_harness PROPERTIES LABELS "reliability")
