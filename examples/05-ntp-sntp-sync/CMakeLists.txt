# CMakeLists.txt for NTP to PTP Synchronization Example
# IEEE 1588-2019 PTP Library - Example 05

cmake_minimum_required(VERSION 3.15)

# Example project
project(IEEE1588_NTP_Example
    VERSION 1.0.0
    DESCRIPTION "NTP (SNTP) Time Source Adapter for IEEE 1588-2019 PTP"
    LANGUAGES CXX
)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find IEEE 1588-2019 library
find_package(IEEE1588_2019 REQUIRED)

# NTP Adapter Library
add_library(ntp_adapter
    ntp_adapter.cpp
    ntp_adapter.hpp
)

target_include_directories(ntp_adapter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${IEEE1588_2019_INCLUDE_DIRS}
)

target_link_libraries(ntp_adapter
    PUBLIC
        IEEE1588_2019::IEEE1588_2019
)

# Platform-specific networking libraries
if(WIN32)
    target_link_libraries(ntp_adapter PUBLIC ws2_32)
elseif(UNIX)
    # No additional libraries needed on Unix
endif()

# NTP to PTP Sync Example
add_executable(ntp_ptp_sync_example
    ntp_ptp_sync_example.cpp
)

target_link_libraries(ntp_ptp_sync_example
    PRIVATE
        ntp_adapter
        IEEE1588_2019::IEEE1588_2019
)

# Installation
install(TARGETS ntp_adapter ntp_ptp_sync_example
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    ntp_adapter.hpp
    README.md
    DESTINATION examples/05-ntp-sntp-sync
)

# Documentation
add_custom_target(ntp_example_doc
    COMMAND ${CMAKE_COMMAND} -E echo "NTP to PTP Synchronization Example"
    COMMAND ${CMAKE_COMMAND} -E echo "Usage: ntp_ptp_sync_example [ntp_server] [poll_interval_s]"
    COMMAND ${CMAKE_COMMAND} -E echo "Example: ntp_ptp_sync_example pool.ntp.org 64"
    VERBATIM
)
