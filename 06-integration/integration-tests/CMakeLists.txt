# Phase 06 Integration Tests CMake
cmake_minimum_required(VERSION 3.16)

set(INTEGRATION_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/test_boundary_clock_integration.cpp
)

add_executable(boundary_clock_integration ${INTEGRATION_SOURCES})
target_include_directories(boundary_clock_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(boundary_clock_integration PRIVATE IEEE1588_2019)

add_test(NAME boundary_clock_integration COMMAND boundary_clock_integration)
