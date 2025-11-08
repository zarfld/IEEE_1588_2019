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
