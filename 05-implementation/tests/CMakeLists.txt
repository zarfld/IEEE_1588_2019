# Phase 05 Implementation TDD Tests - BMCA
# Traceability: REQ-F-002 (Best Master Clock Algorithm)
# Design References: DES-C-031, DES-I-032, DES-D-033

cmake_minimum_required(VERSION 3.16)

# Shared sources used by BMCA tests
set(BMCA_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
)

# Test 1: Basic BMCA selection
add_executable(bmca_tdd_tests
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_basic.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tdd_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tdd_tests PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_red_green_refactor_basic COMMAND bmca_tdd_tests)

# Test 2: Edge comparisons
add_executable(bmca_tdd_edges
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_edges.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tdd_edges PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tdd_edges PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_edge_comparisons COMMAND bmca_tdd_edges)

# Test 3: Selection list behaviors
add_executable(bmca_tdd_selection
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_selection_list.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tdd_selection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tdd_selection PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_selection_list COMMAND bmca_tdd_selection)

# Test 4: Message header validation branches
add_executable(msg_header_validation
    ${CMAKE_CURRENT_LIST_DIR}/test_message_header_validation.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(msg_header_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(msg_header_validation PRIVATE IEEE1588_2019_interface)
add_test(NAME ptp_message_header_validation COMMAND msg_header_validation)

# Test 5: Message bodies validation (error branches)
add_executable(msg_bodies_validation
    ${CMAKE_CURRENT_LIST_DIR}/test_message_bodies_validation.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(msg_bodies_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(msg_bodies_validation PRIVATE IEEE1588_2019_interface)
add_test(NAME ptp_message_bodies_validation COMMAND msg_bodies_validation)

# Test 6: Offset calculation robustness
add_executable(offset_calculation_tests
    ${CMAKE_CURRENT_LIST_DIR}/test_offset_calculation.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(offset_calculation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(offset_calculation_tests PRIVATE IEEE1588_2019_interface)
add_test(NAME offset_calculation_robustness COMMAND offset_calculation_tests)

# Test 7: Fault injection - offset jitter
add_executable(fault_injection_offset
    ${CMAKE_CURRENT_LIST_DIR}/test_fault_injection_offset.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(fault_injection_offset PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(fault_injection_offset PRIVATE IEEE1588_2019_interface)
add_test(NAME fault_injection_offset_jitter COMMAND fault_injection_offset)

# Test 8: Fault injection - BMCA tie behavior
add_executable(fault_injection_bmca
    ${CMAKE_CURRENT_LIST_DIR}/test_fault_injection_bmca.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(fault_injection_bmca PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(fault_injection_bmca PRIVATE IEEE1588_2019_interface)
add_test(NAME fault_injection_bmca_tie COMMAND fault_injection_bmca)

# Test 9: Health/self-test aggregation
add_executable(health_selftest
    ${CMAKE_CURRENT_LIST_DIR}/test_health_selftest.cpp
)
target_include_directories(health_selftest PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(health_selftest PRIVATE IEEE1588_2019_interface)
add_test(NAME health_selftest COMMAND health_selftest)

# Test 10: Timestamp ordering assertions
add_executable(timestamp_ordering_assertions
    ${CMAKE_CURRENT_LIST_DIR}/test_timestamp_ordering_assertions.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(timestamp_ordering_assertions PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(timestamp_ordering_assertions PRIVATE IEEE1588_2019_interface)
add_test(NAME timestamp_ordering_assertions COMMAND timestamp_ordering_assertions)

# Test 11: Foreign master overflow guard
add_executable(foreign_master_overflow
    ${CMAKE_CURRENT_LIST_DIR}/test_foreign_master_overflow.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(foreign_master_overflow PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(foreign_master_overflow PRIVATE IEEE1588_2019_interface)
add_test(NAME foreign_master_overflow_guard COMMAND foreign_master_overflow)

# Test 12: Health heartbeat emission
add_executable(health_heartbeat
    ${CMAKE_CURRENT_LIST_DIR}/test_health_heartbeat.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(health_heartbeat PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(health_heartbeat PRIVATE IEEE1588_2019_interface)
add_test(NAME health_heartbeat_emission COMMAND health_heartbeat)

# Test 13: Sync heuristic tightening
add_executable(sync_heuristic_tightening
    ${CMAKE_CURRENT_LIST_DIR}/test_sync_heuristic_tightening.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(sync_heuristic_tightening PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(sync_heuristic_tightening PRIVATE IEEE1588_2019_interface)
add_test(NAME sync_heuristic_tightening COMMAND sync_heuristic_tightening)

# Test 14: BMCA role assignment integration (RED)
add_executable(bmca_role_assignment
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_role_assignment.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_role_assignment PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_role_assignment PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_role_assignment_integration COMMAND bmca_role_assignment)

# Test 15: BMCA tie passive scenario (RED until passive logic implemented)
add_executable(bmca_tie_passive
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_tie_passive.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tie_passive PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tie_passive PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_tie_passive COMMAND bmca_tie_passive)

# Test 16: Delay_Resp processing
add_executable(delay_resp_processing
    ${CMAKE_CURRENT_LIST_DIR}/test_delay_resp_processing.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(delay_resp_processing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(delay_resp_processing PRIVATE IEEE1588_2019_interface)
add_test(NAME delay_resp_processing COMMAND delay_resp_processing)

# Test 17: Timeout detection
add_executable(timeout_detection
    ${CMAKE_CURRENT_LIST_DIR}/test_timeout_detection.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(timeout_detection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(timeout_detection PRIVATE IEEE1588_2019_interface)
add_test(NAME timeout_detection COMMAND timeout_detection)

# Test 18: Boundary clock routing
add_executable(boundary_clock_routing
    ${CMAKE_CURRENT_LIST_DIR}/test_boundary_clock_routing.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(boundary_clock_routing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(boundary_clock_routing PRIVATE IEEE1588_2019_interface)
add_test(NAME boundary_clock_routing COMMAND boundary_clock_routing)

# Test 19: State-specific actions
add_executable(state_actions
    ${CMAKE_CURRENT_LIST_DIR}/test_state_actions.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(state_actions PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(state_actions PRIVATE IEEE1588_2019_interface)
add_test(NAME state_actions COMMAND state_actions)

# Test 20: Foreign master list management
add_executable(update_foreign_master
    ${CMAKE_CURRENT_LIST_DIR}/test_update_foreign_master.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(update_foreign_master PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(update_foreign_master PRIVATE IEEE1588_2019_interface)
add_test(NAME update_foreign_master COMMAND update_foreign_master)

# Test 21: Calculate offset and delay (T1/T2/T3/T4 algorithm)
add_executable(calculate_offset_and_delay
    ${CMAKE_CURRENT_LIST_DIR}/test_calculate_offset_and_delay.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(calculate_offset_and_delay PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(calculate_offset_and_delay PRIVATE IEEE1588_2019_interface)
add_test(NAME calculate_offset_and_delay COMMAND calculate_offset_and_delay)

message(STATUS "Phase 05 tests configured: bmca_tdd_tests, bmca_tdd_edges, bmca_tdd_selection, msg_header_validation, msg_bodies_validation, offset_calculation_tests, delay_resp_processing, timeout_detection, boundary_clock_routing, state_actions, update_foreign_master, calculate_offset_and_delay")
