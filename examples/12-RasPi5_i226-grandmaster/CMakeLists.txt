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

# Install rules
install(TARGETS ptp_grandmaster
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
