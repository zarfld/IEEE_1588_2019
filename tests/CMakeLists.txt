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

# Additional compilation/API smoke tests
add_executable(clocks_api_compile ${CMAKE_SOURCE_DIR}/test_clocks_api.cpp)
target_include_directories(clocks_api_compile PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clocks_api_compile PRIVATE IEEE1588_2019_interface)
add_test(NAME clocks_api_compile COMMAND clocks_api_compile)

add_executable(clocks_header_compile ${CMAKE_SOURCE_DIR}/test_clocks_compile.cpp)
target_include_directories(clocks_header_compile PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clocks_header_compile PRIVATE IEEE1588_2019_interface)
add_test(NAME clocks_header_compile COMMAND clocks_header_compile)

message(STATUS "IEEE 1588-2019 additional tests: clocks_api_compile, clocks_header_compile")