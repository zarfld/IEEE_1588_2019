# Raspberry Pi 5 + Intel i226 PTP Grandmaster Example
# IEEE 1588-2019 Implementation

cmake_minimum_required(VERSION 3.16)

project(RasPi5_PTP_Grandmaster
    VERSION 0.1.0
    DESCRIPTION "IEEE 1588-2019 PTP Grandmaster for Raspberry Pi 5"
    LANGUAGES CXX
)

# Language standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find the IEEE 1588-2019 library
# Assumes library is installed or built in parent directory
set(IEEE1588_2019_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

# Include directories
include_directories(
    ${IEEE1588_2019_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files
set(GRANDMASTER_SOURCES
    src/ptp_grandmaster.cpp
    src/linux_ptp_hal.cpp
    src/gps_adapter.cpp
    src/rtc_adapter.cpp
    src/drift_observer.cpp
    src/phc_adapter.cpp
    src/network_adapter.cpp
    src/pi_servo.cpp
    src/phc_calibrator.cpp
    src/servo_state_machine.cpp
    # Note: grandmaster_controller.cpp excluded - used only by test_grandmaster_controller_simple
)

# Executable
add_executable(ptp_grandmaster ${GRANDMASTER_SOURCES})

# Link libraries
# Note: Adjust path based on where IEEE1588_2019 library is built
if(TARGET IEEE1588_2019)
    target_link_libraries(ptp_grandmaster IEEE1588_2019)
else()
    # Link against installed library
    find_library(IEEE1588_LIB NAMES IEEE1588_2019 
                 PATHS /usr/local/lib ${CMAKE_BINARY_DIR}/../..)
    if(IEEE1588_LIB)
        target_link_libraries(ptp_grandmaster ${IEEE1588_LIB})
    else()
        message(WARNING "IEEE1588_2019 library not found. Will link during install.")
    endif()
endif()

# System libraries required
target_link_libraries(ptp_grandmaster
    pthread  # Thread support
    rt       # Real-time library
)

# System libraries
target_link_libraries(ptp_grandmaster
    pthread
    rt  # For clock_gettime, clock_settime
)

# Compiler flags for real-time performance
target_compile_options(ptp_grandmaster PRIVATE
    -Wall
    -Wextra
    -O3
    -march=armv8-a  # ARM64 optimization
)

# ===================================================================
# ptp_grandmaster_v2 - REFACTORED ARCHITECTURE (Modular Components)
# ===================================================================

set(ADAPTER_SOURCES
    src/gps_adapter.cpp
    src/rtc_adapter.cpp
    src/drift_observer.cpp
    src/phc_adapter.cpp
    src/network_adapter.cpp
    src/linux_ptp_hal.cpp
)

set(REFACTORED_CONTROLLER_SOURCES
    src/grandmaster_controller.cpp
    src/pi_servo.cpp
    src/phc_calibrator.cpp
    src/servo_state_machine.cpp
)

add_executable(ptp_grandmaster_v2
    src/ptp_grandmaster_v2.cpp
    src/rtc_drift_discipline.cpp
    ${ADAPTER_SOURCES}
    ${REFACTORED_CONTROLLER_SOURCES}
)

target_include_directories(ptp_grandmaster_v2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IEEE1588_2019_INCLUDE_DIR}
)

target_link_libraries(ptp_grandmaster_v2
    pthread  # Thread support
    rt       # Real-time library
)

# Compiler flags for real-time performance
target_compile_options(ptp_grandmaster_v2 PRIVATE
    -Wall
    -Wextra
    -O3
    -march=armv8-a  # ARM64 optimization
)

# Install both versions
install(TARGETS ptp_grandmaster ptp_grandmaster_v2
    RUNTIME DESTINATION /usr/local/bin
)

# Install systemd service files
install(FILES
    etc/systemd/system/ptp4l-repo.service
    DESTINATION /etc/systemd/system
    OPTIONAL
)

# Install configuration files
install(FILES
    etc/chrony/chrony.conf
    DESTINATION /etc/chrony
    OPTIONAL
)

# Documentation
install(FILES
    README.md
    DESTINATION /usr/local/share/doc/ptp_grandmaster
)

# Unit tests
add_executable(test_phc_adapter tests/test_phc_adapter.cpp src/phc_adapter.cpp)
target_link_libraries(test_phc_adapter pthread rt)
target_compile_options(test_phc_adapter PRIVATE -Wall -Wextra -O2)

add_executable(test_pi_servo tests/test_pi_servo.cpp src/pi_servo.cpp)
target_link_libraries(test_pi_servo pthread)
target_compile_options(test_pi_servo PRIVATE -Wall -Wextra -O2)

add_executable(test_phc_calibrator tests/test_phc_calibrator.cpp src/phc_calibrator.cpp src/phc_adapter.cpp)
target_link_libraries(test_phc_calibrator pthread rt)
target_compile_options(test_phc_calibrator PRIVATE -Wall -Wextra -O2)

add_executable(test_servo_state_machine tests/test_servo_state_machine.cpp src/servo_state_machine.cpp)
target_link_libraries(test_servo_state_machine pthread)
target_compile_options(test_servo_state_machine PRIVATE -Wall -Wextra -O2)

add_executable(test_network_adapter tests/test_network_adapter.cpp src/network_adapter.cpp)
target_link_libraries(test_network_adapter pthread rt)
target_compile_options(test_network_adapter PRIVATE -Wall -Wextra -O2)

# Simplified controller test (structural validation only)
add_executable(test_grandmaster_controller_simple
    tests/test_grandmaster_controller_simple.cpp
    src/grandmaster_controller.cpp
    src/gps_adapter.cpp
    src/rtc_adapter.cpp
    src/drift_observer.cpp
    src/rtc_drift_discipline.cpp
    src/phc_adapter.cpp
    src/network_adapter.cpp
    src/pi_servo.cpp
    src/phc_calibrator.cpp
    src/servo_state_machine.cpp
)
target_link_libraries(test_grandmaster_controller_simple pthread rt)
target_compile_options(test_grandmaster_controller_simple PRIVATE -Wall -Wextra -O2)

# PTP Delay Mechanism tests (CRITICAL - blocks slave synchronization)
add_executable(test_ptp_delay_mechanism tests/test_ptp_delay_mechanism.cpp)
target_link_libraries(test_ptp_delay_mechanism pthread)
target_compile_options(test_ptp_delay_mechanism PRIVATE -Wall -Wextra -O2)

# RTC Drift Discipline Integration tests (Priority #1)
add_executable(test_rtc_discipline_integration 
    tests/test_rtc_discipline_integration.cpp
    src/rtc_drift_discipline.cpp
)
target_include_directories(test_rtc_discipline_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_rtc_discipline_integration pthread)
target_compile_options(test_rtc_discipline_integration PRIVATE -Wall -Wextra -O2)


# Print configuration
message(STATUS "")
message(STATUS "Raspberry Pi 5 PTP Grandmaster Configuration")
message(STATUS "=============================================")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Include dirs:    ${IEEE1588_2019_INCLUDE_DIR}")
message(STATUS "")
message(STATUS "Target Hardware:")
message(STATUS "  - Raspberry Pi 5")
message(STATUS "  - Intel i226 NIC (eth1)")
message(STATUS "  - u-blox GPS (/dev/ttyACM0)")
message(STATUS "  - DS3231 RTC (/dev/rtc1)")
message(STATUS "  - PPS Input (/dev/pps0)")
message(STATUS "")

# RTC Discipline tests (Priority 1 - TDD Roadmap 2026-01-15)
add_executable(test_rtc_discipline 
    tests/test_rtc_discipline.cpp
    src/rtc_drift_discipline.cpp
)
target_link_libraries(test_rtc_discipline pthread)
target_compile_options(test_rtc_discipline PRIVATE -Wall -Wextra -O2)

# Real-Time Threading tests (Priority 2 - TDD Roadmap 2026-01-15)
add_executable(test_rt_threading tests/test_rt_threading.cpp)
target_link_libraries(test_rt_threading pthread)
target_compile_options(test_rt_threading PRIVATE -Wall -Wextra -O2)

# Drift Observer tests (TDD RED-GREEN-REFACTOR - 2026-01-16)
# Uses simple assert-based testing pattern (matches test_pi_servo.cpp)
add_executable(test_drift_observer
    tests/test_drift_observer.cpp
    src/drift_observer.cpp
)
target_link_libraries(test_drift_observer pthread)
target_compile_options(test_drift_observer PRIVATE -Wall -Wextra -O2)

# Integration test: RTC Adapter + DriftObserver
add_executable(test_rtc_drift_integration
    tests/test_rtc_drift_integration.cpp
    src/rtc_adapter.cpp
    src/drift_observer.cpp
    src/linux_ptp_hal.cpp
)
target_include_directories(test_rtc_drift_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_rtc_drift_integration pthread)
target_compile_options(test_rtc_drift_integration PRIVATE -Wall -Wextra -O2)

# PTP Delay Mechanism Integration tests (Priority 0 - TDD Roadmap 2026-01-16)
# Tests example's integration with repository's delay mechanism
add_executable(test_delay_integration
    tests/test_delay_integration.cpp
    src/grandmaster_controller.cpp
    src/network_adapter.cpp
    src/gps_adapter.cpp
    src/rtc_adapter.cpp
    src/drift_observer.cpp
    src/rtc_drift_discipline.cpp
    src/phc_adapter.cpp
    src/pi_servo.cpp
    src/phc_calibrator.cpp
    src/servo_state_machine.cpp
    src/linux_ptp_hal.cpp
)
target_include_directories(test_delay_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IEEE1588_2019_INCLUDE_DIR}
)
target_link_libraries(test_delay_integration pthread rt)
target_compile_options(test_delay_integration PRIVATE -Wall -Wextra -O2)
