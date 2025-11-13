# GPS NMEA Synchronization Example

cmake_minimum_required(VERSION 3.16)

# Example requires C++14 (matches main project)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# GPS NMEA Parser Library
# ============================================================================

add_library(gps_nmea_parser STATIC
    nmea_parser.cpp
    gps_time_converter.cpp
)

target_include_directories(gps_nmea_parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(gps_nmea_parser
    PUBLIC
        IEEE1588_2019
)

# ============================================================================
# Serial HAL Implementations (Platform-Specific)
# ============================================================================

# Windows Serial HAL
if(WIN32)
    add_library(serial_hal_windows STATIC
        serial_hal_windows.cpp
    )
    
    target_include_directories(serial_hal_windows
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # No additional libraries needed (Win32 API is system)
endif()

# Linux Serial HAL
if(UNIX AND NOT APPLE)
    add_library(serial_hal_linux STATIC
        serial_hal_linux.cpp
    )
    
    target_include_directories(serial_hal_linux
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # No additional libraries needed (termios is system)
endif()

# ============================================================================
# GPS NMEA Synchronization Example Application
# ============================================================================

add_executable(gps_nmea_sync_example
    gps_nmea_sync_example.cpp
)

target_include_directories(gps_nmea_sync_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(gps_nmea_sync_example
    PRIVATE
        IEEE1588_2019
        gps_nmea_parser
)

# Link platform-specific serial HAL
if(WIN32)
    target_link_libraries(gps_nmea_sync_example PRIVATE serial_hal_windows)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(gps_nmea_sync_example PRIVATE serial_hal_linux)
endif()

# ============================================================================
# Unit Tests (if testing enabled)
# ============================================================================

if(BUILD_TESTING)
    # NMEA Parser Tests
    add_executable(test_nmea_parser
        tests/test_nmea_parser.cpp
    )
    
    target_include_directories(test_nmea_parser
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(test_nmea_parser
        PRIVATE
            gps_nmea_parser
    )
    
    add_test(NAME test_nmea_parser COMMAND test_nmea_parser)
    set_tests_properties(test_nmea_parser PROPERTIES
        LABELS "gps;nmea;parser"
        TIMEOUT 30
    )
    
    # GPS Time Converter Tests
    add_executable(test_gps_time_converter
        tests/test_gps_time_converter.cpp
    )
    
    target_include_directories(test_gps_time_converter
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(test_gps_time_converter
        PRIVATE
            gps_nmea_parser
    )
    
    add_test(NAME test_gps_time_converter COMMAND test_gps_time_converter)
    set_tests_properties(test_gps_time_converter PROPERTIES
        LABELS "gps;time;converter"
        TIMEOUT 30
    )
    
    # Integration Tests
    add_executable(test_gps_integration
        tests/test_integration.cpp
    )
    
    target_include_directories(test_gps_integration
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(test_gps_integration
        PRIVATE
            gps_nmea_parser
            IEEE1588_2019
    )
    
    add_test(NAME test_gps_integration COMMAND test_gps_integration)
    set_tests_properties(test_gps_integration PROPERTIES
        LABELS "gps;integration"
        TIMEOUT 60
    )
    
    # Copy real GPS log files for testing
    file(COPY ${CMAKE_SOURCE_DIR}/docs/Serial_NMEA_example/Log1.log
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test_data)
    file(COPY ${CMAKE_SOURCE_DIR}/docs/Serial_NMEA_example/log2.log
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test_data)
    file(COPY ${CMAKE_SOURCE_DIR}/docs/Serial_NMEA_example/log3.log
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test_data)
    
    message(STATUS "GPS NMEA tests configured with real log files")
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS gps_nmea_sync_example
    RUNTIME DESTINATION bin/examples
    COMPONENT examples
)

install(FILES
    README.md
    GPS_NMEA_Integration_Spec.md
    GPS_NMEA_Specification_Refinement.md
    DESTINATION share/doc/IEEE1588_2019/examples/gps-nmea-sync
    COMPONENT documentation
)

# ============================================================================
# Status Messages
# ============================================================================

message(STATUS "GPS NMEA Synchronization Example configured")
if(WIN32)
    message(STATUS "  Serial HAL: Windows (Win32 API)")
elseif(UNIX AND NOT APPLE)
    message(STATUS "  Serial HAL: Linux (termios)")
else()
    message(WARNING "  Serial HAL: Not configured for this platform")
endif()
