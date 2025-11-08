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
)
target_include_directories(sync_heuristic_tightening PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(sync_heuristic_tightening PRIVATE IEEE1588_2019_interface)
add_test(NAME sync_heuristic_tightening COMMAND sync_heuristic_tightening)

message(STATUS "Phase 05 tests configured: bmca_tdd_tests, bmca_tdd_edges, bmca_tdd_selection, msg_header_validation, msg_bodies_validation, offset_calculation_tests")
