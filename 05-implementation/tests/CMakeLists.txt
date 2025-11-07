# Phase 05 Implementation TDD Tests - BMCA
# Traceability: REQ-F-002 (Best Master Clock Algorithm)
# Design References: DES-C-031, DES-I-032, DES-D-033

cmake_minimum_required(VERSION 3.16)

set(PHASE05_TEST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_basic.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
)

add_executable(bmca_tdd_tests ${PHASE05_TEST_SOURCES})

target_include_directories(bmca_tdd_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)

# Link only the interface layer (hardware agnostic)
target_link_libraries(bmca_tdd_tests PRIVATE IEEE1588_2019_interface)

add_test(NAME bmca_red_green_refactor_basic COMMAND bmca_tdd_tests)

message(STATUS "Phase 05 BMCA TDD tests configured: bmca_tdd_tests")
