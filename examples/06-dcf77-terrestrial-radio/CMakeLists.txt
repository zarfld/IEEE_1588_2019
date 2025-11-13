# CMakeLists.txt for DCF77 to PTP Synchronization Example
# IEEE 1588-2019 PTP Library - Example 06

cmake_minimum_required(VERSION 3.15)

# Example project
project(IEEE1588_DCF77_Example
    VERSION 1.0.0
    DESCRIPTION "DCF77 Terrestrial Radio Time Source Adapter for IEEE 1588-2019 PTP"
    LANGUAGES CXX
)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find IEEE 1588-2019 library
find_package(IEEE1588_2019 REQUIRED)

# DCF77 Adapter Library
add_library(dcf77_adapter
    dcf77_adapter.cpp
    dcf77_adapter.hpp
)

target_include_directories(dcf77_adapter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${IEEE1588_2019_INCLUDE_DIRS}
)

target_link_libraries(dcf77_adapter
    PUBLIC
        IEEE1588_2019::IEEE1588_2019
)

# Platform-specific GPIO libraries
# NOTE: Real implementation would link to ESP32 IDF, Arduino, or platform GPIO library
# This is a placeholder for the example
if(ESP32)
    # target_link_libraries(dcf77_adapter PUBLIC idf::driver)
endif()

# DCF77 to PTP Sync Example
add_executable(dcf77_ptp_sync_example
    dcf77_ptp_sync_example.cpp
)

target_link_libraries(dcf77_ptp_sync_example
    PRIVATE
        dcf77_adapter
        IEEE1588_2019::IEEE1588_2019
)

# Installation
install(TARGETS dcf77_adapter dcf77_ptp_sync_example
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    dcf77_adapter.hpp
    README.md
    DESTINATION examples/06-dcf77-terrestrial-radio
)

# Documentation
add_custom_target(dcf77_example_doc
    COMMAND ${CMAKE_COMMAND} -E echo "DCF77 to PTP Synchronization Example"
    COMMAND ${CMAKE_COMMAND} -E echo "Hardware: DCF77 receiver module + ESP32/Arduino"
    COMMAND ${CMAKE_COMMAND} -E echo "Coverage: ~2000 km from Mainflingen, Germany"
    COMMAND ${CMAKE_COMMAND} -E echo "Usage: dcf77_ptp_sync_example [gpio_pin] [invert_signal]"
    COMMAND ${CMAKE_COMMAND} -E echo "Example: dcf77_ptp_sync_example 4 0"
    VERBATIM
)
