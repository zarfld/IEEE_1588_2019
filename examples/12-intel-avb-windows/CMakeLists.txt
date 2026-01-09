# CMakeLists.txt for Intel AVB Windows PTP Example
# IEEE 1588-2019 compliant implementation using Intel AVB Filter Driver

cmake_minimum_required(VERSION 3.16)

project(IntelAVBPTPExample
    VERSION 1.0.0
    DESCRIPTION "IEEE 1588-2019 PTP Example using Intel AVB Filter Driver"
    LANGUAGES CXX
)

# Require C++17 for modern features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-only example
if(NOT WIN32)
    message(WARNING "Intel AVB example requires Windows platform - skipping")
    return()
endif()

# Find Intel AVB Filter Driver headers
# Try multiple possible locations
set(INTEL_AVB_INCLUDE_PATHS
    "${CMAKE_SOURCE_DIR}/../IntelAvbFilter/include"
    "${CMAKE_SOURCE_DIR}/../../IntelAvbFilter/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../IntelAvbFilter/include"
    "C:/Users/dzarf/source/repos/IntelAvbFilter/include"
)

find_path(INTEL_AVB_INCLUDE_DIR
    NAMES avb_ioctl.h
    PATHS ${INTEL_AVB_INCLUDE_PATHS}
    DOC "Intel AVB Filter Driver include directory"
)

if(NOT INTEL_AVB_INCLUDE_DIR)
    message(WARNING "Intel AVB Filter Driver headers not found - skipping example")
    message(STATUS "Searched paths: ${INTEL_AVB_INCLUDE_PATHS}")
    message(STATUS "Please ensure IntelAvbFilter repository is cloned as sibling to IEEE_1588_2019")
    return()
endif()

message(STATUS "Found Intel AVB headers: ${INTEL_AVB_INCLUDE_DIR}")

# Intel AVB HAL Library
add_library(intel_avb_hal STATIC
    intel_avb_hal.cpp
    intel_avb_hal.hpp
)

target_include_directories(intel_avb_hal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${INTEL_AVB_INCLUDE_DIR}
)

target_compile_definitions(intel_avb_hal
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

# Windows libraries required for DeviceIoControl
target_link_libraries(intel_avb_hal
    PUBLIC
        setupapi
)

# PTP Example Executable
add_executable(intel_avb_ptp_example
    intel_avb_ptp_example.cpp
)

target_link_libraries(intel_avb_ptp_example
    PRIVATE
        intel_avb_hal
)

target_compile_definitions(intel_avb_ptp_example
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

# Diagnostic tool for testing register access
add_executable(test_register_access
    test_register_access.cpp
)

target_link_libraries(test_register_access
    PRIVATE
        intel_avb_hal
        setupapi
)

# Set output directories
set_target_properties(intel_avb_ptp_example
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Release"
)

set_target_properties(test_register_access
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Release"
)

# Test application for timestamp functionality validation
add_executable(test_timestamp_functionality
    test_timestamp_functionality.cpp
    intel_avb_hal.cpp
)

target_link_libraries(test_timestamp_functionality PRIVATE setupapi)

set_target_properties(test_timestamp_functionality
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Release"
)

# Direct IOCTL test to validate actual IOCTL behavior (matches reference test)
add_executable(test_ioctl_direct
    test_ioctl_direct.cpp
)

target_include_directories(test_ioctl_direct
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${INTEL_AVB_INCLUDE_DIR}
)

target_link_libraries(test_ioctl_direct PRIVATE setupapi)

set_target_properties(test_ioctl_direct
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/examples/12-intel-avb-windows/Release"
)

# Enable warnings
if(MSVC)
    target_compile_options(intel_avb_hal PRIVATE /W4 /WX-)
    target_compile_options(intel_avb_ptp_example PRIVATE /W4 /WX-)
    target_compile_options(test_register_access PRIVATE /W4 /WX-)
    target_compile_options(test_timestamp_functionality PRIVATE /W4 /WX-)
    target_compile_options(test_ioctl_direct PRIVATE /W4 /WX-)
else()
    target_compile_options(intel_avb_hal PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(intel_avb_ptp_example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_register_access PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_timestamp_functionality PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_ioctl_direct PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation rules (optional)
install(TARGETS intel_avb_ptp_example test_register_access test_timestamp_functionality test_ioctl_direct
    RUNTIME DESTINATION bin/examples
)

install(FILES
    README.md
    intel_avb_hal.hpp
    DESTINATION share/doc/ieee1588-2019/examples/12-intel-avb-windows
)

# Add test target (requires Administrator privileges)
# Note: This test will fail without proper hardware and driver setup
add_test(
    NAME intel_avb_hardware_test
    COMMAND intel_avb_ptp_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(intel_avb_hardware_test
    PROPERTIES
        LABELS "hardware;windows;ptp"
        DISABLED TRUE  # Disabled by default (requires hardware + admin)
)

# Print build information
message(STATUS "=== Intel AVB PTP Example Configuration ===")
message(STATUS "  Example:     Enabled")
message(STATUS "  Platform:    Windows")
message(STATUS "  AVB Headers: ${INTEL_AVB_INCLUDE_DIR}")
message(STATUS "  Build Type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "==========================================")
