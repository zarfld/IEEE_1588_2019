# IEEE 1588-2019 PTP v2.1 Tests
# Unit tests and integration tests for PTP core components

cmake_minimum_required(VERSION 3.16)

set(TEST_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/test_state_machine_basic.cpp
	${CMAKE_SOURCE_DIR}/src/clocks.cpp
)

add_executable(ptp_state_machine_tests ${TEST_SOURCES})
target_include_directories(ptp_state_machine_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ptp_state_machine_tests PRIVATE IEEE1588_2019_interface)

add_test(NAME ptp_state_machine_basic COMMAND ptp_state_machine_tests)

message(STATUS "IEEE 1588-2019 PTP tests configured: ptp_state_machine_tests")