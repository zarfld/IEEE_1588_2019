# IEEE 1588-2019 PTP v2.1 Tests
# Unit tests and integration tests for PTP core components

cmake_minimum_required(VERSION 3.16)

set(TEST_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/test_state_machine_basic.cpp
	${CMAKE_CURRENT_LIST_DIR}/test_messages_validate.cpp
	${CMAKE_CURRENT_LIST_DIR}/test_types_timestamp.cpp
)

add_executable(ptp_state_machine_tests ${TEST_SOURCES})
target_include_directories(ptp_state_machine_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ptp_state_machine_tests PRIVATE IEEE1588_2019)

add_test(NAME ptp_state_machine_basic COMMAND ptp_state_machine_tests)

# Register additional focused tests as separate CTest entries invoking the same binary
add_test(NAME ptp_messages_validate COMMAND ptp_state_machine_tests --case messages)
add_test(NAME ptp_types_timestamp COMMAND ptp_state_machine_tests --case timestamp)

# New RED phase test for REQ-F-003 (Offset Calculation). Separate executable for isolation.
# Separate executable for RED phase offset calculation test (isolated main)
add_executable(ptp_offset_calc_red ${CMAKE_CURRENT_LIST_DIR}/test_offset_calc_red.cpp)
target_include_directories(ptp_offset_calc_red PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ptp_offset_calc_red PRIVATE IEEE1588_2019)
add_test(NAME ptp_offset_calc_red COMMAND ptp_offset_calc_red)

message(STATUS "IEEE 1588-2019 PTP tests configured: ptp_state_machine_tests")

# Additional compilation/API smoke tests
add_executable(ptp_state_machine_heuristic_negative ${CMAKE_CURRENT_LIST_DIR}/test_state_machine_heuristic_negative.cpp)
target_include_directories(ptp_state_machine_heuristic_negative PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ptp_state_machine_heuristic_negative PRIVATE IEEE1588_2019)
add_test(NAME ptp_state_machine_heuristic_negative COMMAND ptp_state_machine_heuristic_negative)

# Additional compilation/API smoke tests
add_executable(clocks_api_compile ${CMAKE_SOURCE_DIR}/test_clocks_api.cpp)
target_include_directories(clocks_api_compile PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clocks_api_compile PRIVATE IEEE1588_2019)
add_test(NAME clocks_api_compile COMMAND clocks_api_compile)

add_executable(clocks_header_compile ${CMAKE_SOURCE_DIR}/test_clocks_compile.cpp)
target_include_directories(clocks_header_compile PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clocks_header_compile PRIVATE IEEE1588_2019)
add_test(NAME clocks_header_compile COMMAND clocks_header_compile)

message(STATUS "IEEE 1588-2019 additional tests: clocks_api_compile, clocks_header_compile")