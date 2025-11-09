# IEEE 1588-2019 PTP v2.1 Tests
# Unit tests and integration tests for PTP core components

cmake_minimum_required(VERSION 3.16)

set(TEST_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/test_state_machine_basic.cpp
	${CMAKE_CURRENT_LIST_DIR}/test_messages_validate.cpp
	${CMAKE_CURRENT_LIST_DIR}/test_types_timestamp.cpp
	${CMAKE_CURRENT_LIST_DIR}/test_p2p_delay_red.cpp
)

add_executable(ptp_state_machine_tests ${TEST_SOURCES})
target_include_directories(ptp_state_machine_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ptp_state_machine_tests PRIVATE IEEE1588_2019)
target_compile_definitions(ptp_state_machine_tests PRIVATE INTEGRATED_TEST_SUITE)

add_test(NAME ptp_state_machine_basic COMMAND ptp_state_machine_tests)

# Register additional focused tests as separate CTest entries invoking the same binary
add_test(NAME ptp_messages_validate COMMAND ptp_state_machine_tests --case messages)
add_test(NAME ptp_types_timestamp COMMAND ptp_state_machine_tests --case timestamp)

# RED test for P2P delay mechanism (REQ-F-204)
add_test(NAME ptp_p2p_delay_red COMMAND ptp_state_machine_tests --case p2p-delay-red)

# New RED phase test for REQ-F-003 (Offset Calculation). Separate executable for isolation.
# Separate executable for RED phase offset calculation test (isolated main)
add_executable(ptp_offset_calc_red ${CMAKE_CURRENT_LIST_DIR}/test_offset_calc_red.cpp)
target_include_directories(ptp_offset_calc_red PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ptp_offset_calc_red PRIVATE IEEE1588_2019)
add_test(NAME ptp_offset_calc_red COMMAND ptp_offset_calc_red)

# GAP-FOREIGN-001 RED phase test for foreign master list management
add_executable(foreign_master_list_red ${CMAKE_CURRENT_LIST_DIR}/test_foreign_master_list_red.cpp)
target_include_directories(foreign_master_list_red PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(foreign_master_list_red PRIVATE IEEE1588_2019)
add_test(NAME foreign_master_list_red COMMAND foreign_master_list_red)

# GAP-FOREIGN-001 GREEN verification test for pruning implementation
add_executable(foreign_master_pruning_verify ${CMAKE_CURRENT_LIST_DIR}/test_foreign_master_pruning_verify.cpp)
target_include_directories(foreign_master_pruning_verify PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(foreign_master_pruning_verify PRIVATE IEEE1588_2019)
add_test(NAME foreign_master_pruning_verify COMMAND foreign_master_pruning_verify)

# GAP-OFFSET-TEST-001 RED phase acceptance test for offset calculation
add_executable(offset_calculation_red ${CMAKE_CURRENT_LIST_DIR}/test_offset_calculation_red.cpp)
target_include_directories(offset_calculation_red PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(offset_calculation_red PRIVATE IEEE1588_2019)
add_test(NAME offset_calculation_red COMMAND offset_calculation_red)

# GAP-PDELAY-001 RED phase acceptance test for peer delay mechanism
add_executable(pdelay_mechanism_red ${CMAKE_CURRENT_LIST_DIR}/test_pdelay_mechanism_red.cpp)
target_include_directories(pdelay_mechanism_red PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(pdelay_mechanism_red PRIVATE IEEE1588_2019)
add_test(NAME pdelay_mechanism_red COMMAND pdelay_mechanism_red)

# GAP-TRANSP-001 Simple verification test for transparent clock
add_executable(transparent_clock_simple ${CMAKE_CURRENT_LIST_DIR}/test_transparent_clock_simple.cpp)
target_include_directories(transparent_clock_simple PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(transparent_clock_simple PRIVATE IEEE1588_2019)
add_test(NAME transparent_clock_simple COMMAND transparent_clock_simple)

message(STATUS "IEEE 1588-2019 PTP tests configured: ptp_state_machine_tests")

# Additional compilation/API smoke tests
add_executable(ptp_state_machine_heuristic_negative ${CMAKE_CURRENT_LIST_DIR}/test_state_machine_heuristic_negative.cpp)
target_include_directories(ptp_state_machine_heuristic_negative PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ptp_state_machine_heuristic_negative PRIVATE IEEE1588_2019)
add_test(NAME ptp_state_machine_heuristic_negative COMMAND ptp_state_machine_heuristic_negative)

# Additional compilation/API smoke tests
add_executable(rounding_bias_characterization ${CMAKE_CURRENT_LIST_DIR}/test_rounding_bias.cpp)
target_include_directories(rounding_bias_characterization PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rounding_bias_characterization PRIVATE IEEE1588_2019)
add_test(NAME rounding_bias_characterization COMMAND rounding_bias_characterization)

# Offset clamp boundary test (FM-002/FM-013 verification)
add_executable(offset_clamp_boundary ${CMAKE_CURRENT_LIST_DIR}/test_offset_clamp_boundary.cpp)
target_include_directories(offset_clamp_boundary PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(offset_clamp_boundary PRIVATE IEEE1588_2019)
add_test(NAME offset_clamp_boundary COMMAND offset_clamp_boundary)

# Health snapshot test (health API verification)
add_executable(health_snapshot ${CMAKE_CURRENT_LIST_DIR}/test_health_snapshot.cpp)
target_include_directories(health_snapshot PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(health_snapshot PRIVATE IEEE1588_2019)
add_test(NAME health_snapshot COMMAND health_snapshot)
add_executable(clocks_api_compile ${CMAKE_SOURCE_DIR}/test_clocks_api.cpp)
target_include_directories(clocks_api_compile PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clocks_api_compile PRIVATE IEEE1588_2019)
add_test(NAME clocks_api_compile COMMAND clocks_api_compile)

add_executable(clocks_header_compile ${CMAKE_SOURCE_DIR}/test_clocks_compile.cpp)
target_include_directories(clocks_header_compile PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clocks_header_compile PRIVATE IEEE1588_2019)
add_test(NAME clocks_header_compile COMMAND clocks_header_compile)

message(STATUS "IEEE 1588-2019 additional tests: clocks_api_compile, clocks_header_compile")