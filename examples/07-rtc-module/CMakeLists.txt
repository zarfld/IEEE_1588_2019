# RTC Module Time Source Example
# Demonstrates RTC as both time source and sink

cmake_minimum_required(VERSION 3.15)

# RTC Adapter library
add_library(rtc_adapter STATIC
    rtc_adapter.cpp
    rtc_adapter.hpp
)

target_include_directories(rtc_adapter PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtc_adapter PUBLIC
    ieee1588_2019_ptp
)

# RTC PTP Sync Example executable
add_executable(rtc_ptp_sync_example
    rtc_ptp_sync_example.cpp
)

target_link_libraries(rtc_ptp_sync_example PRIVATE
    rtc_adapter
    ieee1588_2019_ptp
)

target_include_directories(rtc_ptp_sync_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation
install(TARGETS rtc_ptp_sync_example
    RUNTIME DESTINATION bin/examples
)

install(FILES
    rtc_adapter.hpp
    DESTINATION include/examples/rtc
)

message(STATUS "RTC Module Synchronization Example configured")
message(STATUS "  Hardware: DS3231/DS1307/PCF8523 RTC on I2C")
message(STATUS "  Run: ./rtc_ptp_sync_example")
