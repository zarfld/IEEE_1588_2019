# Multi-Source Time Synchronization with BMCA
# Demonstrates automatic selection between GPS, NTP, DCF77, and RTC

cmake_minimum_required(VERSION 3.15)

# Multi-source BMCA example executable
add_executable(multi_source_sync_example
    multi_source_sync_example.cpp
)

target_link_libraries(multi_source_sync_example PRIVATE
    gps_adapter          # From examples/04-gps-nmea-sync
    ntp_adapter          # From examples/05-ntp-sntp-sync
    dcf77_adapter        # From examples/06-dcf77-terrestrial-radio
    rtc_adapter          # From examples/07-rtc-module
    ieee1588_2019_ptp
)

target_include_directories(multi_source_sync_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/examples/04-gps-nmea-sync
    ${CMAKE_SOURCE_DIR}/examples/05-ntp-sntp-sync
    ${CMAKE_SOURCE_DIR}/examples/06-dcf77-terrestrial-radio
    ${CMAKE_SOURCE_DIR}/examples/07-rtc-module
)

# Installation
install(TARGETS multi_source_sync_example
    RUNTIME DESTINATION bin/examples
)

message(STATUS "Multi-Source BMCA Synchronization Example configured")
message(STATUS "  Sources: GPS + NTP + DCF77 + RTC")
message(STATUS "  Selection: IEEE 1588-2019 Best Master Clock Algorithm")
message(STATUS "  Run: ./multi_source_sync_example")
