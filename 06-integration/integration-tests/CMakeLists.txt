# Phase 06 Integration Tests CMake
cmake_minimum_required(VERSION 3.16)

# Task 1: BMCA Runtime Integration Test
add_executable(bmca_runtime_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_runtime_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/bmca_integration.cpp
)
target_include_directories(bmca_runtime_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bmca_runtime_integration PRIVATE IEEE1588_2019)
add_test(NAME bmca_runtime_integration COMMAND bmca_runtime_integration)
set_tests_properties(bmca_runtime_integration PROPERTIES LABELS "integration")

# Task 2: Sync Accuracy Integration Test
add_executable(sync_accuracy_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_sync_accuracy_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/sync_integration.cpp
)
target_include_directories(sync_accuracy_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(sync_accuracy_integration PRIVATE IEEE1588_2019)
add_test(NAME sync_accuracy_integration COMMAND sync_accuracy_integration)
set_tests_properties(sync_accuracy_integration PROPERTIES LABELS "integration")

# Task 3: Servo Behavior Integration Test
add_executable(servo_behavior_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_servo_behavior_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/servo_integration.cpp
)
target_include_directories(servo_behavior_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(servo_behavior_integration PRIVATE IEEE1588_2019)
add_test(NAME servo_behavior_integration COMMAND servo_behavior_integration)
set_tests_properties(servo_behavior_integration PROPERTIES LABELS "integration")

# Task 4: Message Flow Integration Test
add_executable(message_flow_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_message_flow_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/message_flow_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/bmca_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/sync_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/servo_integration.cpp
)
target_include_directories(message_flow_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(message_flow_integration PRIVATE IEEE1588_2019)
add_test(NAME message_flow_integration COMMAND message_flow_integration)
set_tests_properties(message_flow_integration PROPERTIES LABELS "integration")

# Task 5: End-to-End Validation Test
add_executable(end_to_end_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_end_to_end_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/message_flow_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/bmca_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/sync_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/servo_integration.cpp
)
target_include_directories(end_to_end_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(end_to_end_integration PRIVATE IEEE1588_2019)
add_test(NAME end_to_end_integration COMMAND end_to_end_integration)
set_tests_properties(end_to_end_integration PROPERTIES LABELS "integration;end-to-end")

# Task 6: Error Recovery Integration Test
add_executable(error_recovery_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_error_recovery_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/message_flow_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/bmca_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/sync_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/servo_integration.cpp
)
target_include_directories(error_recovery_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(error_recovery_integration PRIVATE IEEE1588_2019)
add_test(NAME error_recovery_integration COMMAND error_recovery_integration)
set_tests_properties(error_recovery_integration PROPERTIES LABELS "integration;error-recovery")

# Task 7: Performance Profiling Integration Test
add_executable(performance_integration
    ${CMAKE_CURRENT_LIST_DIR}/test_performance_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/message_flow_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/bmca_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/sync_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/servo_integration.cpp
)
target_include_directories(performance_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(performance_integration PRIVATE IEEE1588_2019)
add_test(NAME performance_integration COMMAND performance_integration)
set_tests_properties(performance_integration PROPERTIES LABELS "integration;performance")

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

# Ensure a stable, CI-friendly output directory for SRG CSV
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/reliability)

add_test(NAME reliability_harness
    COMMAND reliability_harness 200 ${CMAKE_BINARY_DIR}/reliability/srg_failures.csv)
set_tests_properties(reliability_harness PROPERTIES LABELS "reliability")
