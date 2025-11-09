# IEEE 1588-2019 PTP v2.1 Implementation
# Hardware-agnostic Precision Time Protocol
# Copyright (C) 2025 OpenAvnu Project

cmake_minimum_required(VERSION 3.16)

project(IEEE1588_2019_PTP
    VERSION 1.0.0
    DESCRIPTION "IEEE 1588-2019 Precision Time Protocol v2.1 Implementation"
    LANGUAGES CXX
)

# Language standards
# Allow CI to override CMAKE_CXX_STANDARD/CMAKE_C_STANDARD (e.g., C++17/C11)
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
set(IEEE1588_2019_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files (will be populated as implementation progresses)
set(IEEE1588_2019_SOURCES
    src/clocks.cpp
    05-implementation/src/bmca.cpp
    # src/types.cpp
    # src/messages.cpp
    # src/algorithms.cpp
    # src/security.cpp
    # src/management.cpp
    # src/hardware.cpp
)

# Header files
set(IEEE1588_2019_HEADERS
    include/IEEE/1588/PTP/2019/ieee1588_2019.hpp
    include/IEEE/1588/PTP/2019/namespace.hpp
    include/IEEE/1588/PTP/2019/types.hpp
    include/IEEE/1588/PTP/2019/messages.hpp
    include/IEEE/1588/PTP/2019/profile.hpp
    include/IEEE/1588/PTP/2019/profile_adapter.hpp
    include/clocks.hpp
    # include/IEEE/1588/PTP/2019/clock.hpp
    # include/IEEE/1588/PTP/2019/algorithms.hpp
    # include/IEEE/1588/PTP/2019/security.hpp
    # include/IEEE/1588/PTP/2019/management.hpp
    # include/IEEE/1588/PTP/2019/hardware.hpp
)

# Create interface library for header-only components
add_library(IEEE1588_2019_interface INTERFACE)
target_include_directories(IEEE1588_2019_interface
    INTERFACE
        $<BUILD_INTERFACE:${IEEE1588_2019_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Create compiled library for implemented sources (clocks.cpp, etc.)
add_library(IEEE1588_2019 ${IEEE1588_2019_SOURCES} ${IEEE1588_2019_HEADERS})
target_include_directories(IEEE1588_2019
    PUBLIC
        $<BUILD_INTERFACE:${IEEE1588_2019_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(IEEE1588_2019 PUBLIC IEEE1588_2019_interface)

# Phase 06 reliability tool: Laplace trend analyzer (non-gating)
add_executable(laplace_trend
    06-integration/integration-tests/reliability/laplace_trend.cpp
)
set_target_properties(laplace_trend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/reliability"
)

# Phase 06 reliability tool: Dashboard metrics (non-gating)
add_executable(reliability_dashboard
    06-integration/integration-tests/reliability/reliability_dashboard.cpp
)
set_target_properties(reliability_dashboard PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/reliability"
)

# Phase 06 reliability tool: SRG exporter (non-gating)
add_executable(srg_export
    06-integration/integration-tests/reliability/srg_export.cpp
)
set_target_properties(srg_export PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/reliability"
)

# Testing support
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    # Phase 05 TDD tests
    add_subdirectory(05-implementation/tests)
    # Phase 06 Integration tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/06-integration/integration-tests/CMakeLists.txt)
        add_subdirectory(06-integration/integration-tests)
    endif()
    # Phase 07 tools (non-gating)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/07-verification-validation/tools/srg_fit_stub.cpp)
        add_executable(srg_fit_stub 07-verification-validation/tools/srg_fit_stub.cpp)
        set_target_properties(srg_fit_stub PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/verification"
        )
        add_test(NAME srg-fit-stub
            COMMAND $<TARGET_FILE:srg_fit_stub> "${CMAKE_BINARY_DIR}/reliability/srg_export.csv")
        set_tests_properties(srg-fit-stub PROPERTIES
            LABELS "verification;srg"
            PASS_REGULAR_EXPRESSION "SRG_FIT:")
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/07-verification-validation/tools/srg_fit.cpp)
        add_executable(srg_fit 07-verification-validation/tools/srg_fit.cpp)
        set_target_properties(srg_fit PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/verification"
        )
        add_test(NAME srg-fit
            COMMAND $<TARGET_FILE:srg_fit> "${CMAKE_BINARY_DIR}/reliability/srg_export.csv")
        set_tests_properties(srg-fit PROPERTIES
            LABELS "verification;srg"
            PASS_REGULAR_EXPRESSION "SRG_FIT:")
    endif()
    # Non-failing reliability trend observation test
    add_test(NAME reliability-trend
        COMMAND $<TARGET_FILE:laplace_trend> "${CMAKE_BINARY_DIR}/reliability/reliability_history.csv")
    set_tests_properties(reliability-trend PROPERTIES
        LABELS "reliability;trend"
        PASS_REGULAR_EXPRESSION "LAPLACE_TREND:")
    add_test(NAME reliability-dashboard
        COMMAND $<TARGET_FILE:reliability_dashboard> "${CMAKE_BINARY_DIR}/reliability/reliability_history.csv")
    set_tests_properties(reliability-dashboard PROPERTIES
        LABELS "reliability;dashboard"
        PASS_REGULAR_EXPRESSION "DASHBOARD:")
    add_test(NAME srg-export
        COMMAND $<TARGET_FILE:srg_export> "${CMAKE_BINARY_DIR}/reliability/srg_failures.csv" "${CMAKE_BINARY_DIR}/reliability/srg_export.csv")
    set_tests_properties(srg-export PROPERTIES
        LABELS "reliability;srg"
        PASS_REGULAR_EXPRESSION "SRG_EXPORT:")
    # Register example smoke test as part of CTest if examples enabled
    if(IEEE1588_2019_BUILD_EXAMPLES)
        # Ensure example targets are created before registering tests
        # The examples are added below; defer test registration until after examples
        set(_REGISTER_EXAMPLE_TEST TRUE)
    endif()
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(
    DIRECTORY ${IEEE1588_2019_INCLUDE_DIR}/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(
    TARGETS IEEE1588_2019_interface
    EXPORT IEEE1588_2019Targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Examples (optional build)
option(IEEE1588_2019_BUILD_EXAMPLES "Build IEEE 1588-2019 examples" ON)

if(IEEE1588_2019_BUILD_EXAMPLES)
    # Basic types example
    add_executable(ieee1588_2019_basic_example
        examples/basic_types_example.cpp
    )
    
    # Time-sensitive design example
    add_executable(ieee1588_2019_time_sensitive_example
        examples/time_sensitive_types_example.cpp
    )
    
    # Simple verification test
    add_executable(ieee1588_2019_simple_test
        examples/simple_clock_test.cpp
    )
    
    target_link_libraries(ieee1588_2019_basic_example
        PRIVATE IEEE1588_2019_interface
    )
    
    target_link_libraries(ieee1588_2019_time_sensitive_example
        PRIVATE IEEE1588_2019_interface
    )
    
    target_link_libraries(ieee1588_2019_simple_test
        PRIVATE IEEE1588_2019_interface
    )
    
    # Set output directory
    set_target_properties(ieee1588_2019_basic_example ieee1588_2019_time_sensitive_example ieee1588_2019_simple_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )

    # If testing is enabled, register the simple test executable as a CTest test
    if(BUILD_TESTING)
        add_test(NAME simple_example_smoke COMMAND $<TARGET_FILE:ieee1588_2019_simple_test>)
    endif()
endif()

# Export configuration
install(
    EXPORT IEEE1588_2019Targets
    FILE IEEE1588_2019Targets.cmake
    NAMESPACE IEEE1588_2019::
    DESTINATION lib/cmake/IEEE1588_2019
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    IEEE1588_2019ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/IEEE1588_2019Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/IEEE1588_2019Config.cmake
    INSTALL_DESTINATION lib/cmake/IEEE1588_2019
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/IEEE1588_2019Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/IEEE1588_2019ConfigVersion.cmake
    DESTINATION lib/cmake/IEEE1588_2019
)