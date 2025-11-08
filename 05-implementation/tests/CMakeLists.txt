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

message(STATUS "Phase 05 tests configured: bmca_tdd_tests, bmca_tdd_edges, bmca_tdd_selection, msg_header_validation, msg_bodies_validation, offset_calculation_tests")
