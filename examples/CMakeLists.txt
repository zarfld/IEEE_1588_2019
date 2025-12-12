# IEEE 1588-2019 PTP v2.1 Examples
# Example applications demonstrating time-sensitive design principles

# Phase 08 - Transition Examples
# Complete working examples for learning and integration
add_subdirectory(01-basic-ptp-slave)
add_subdirectory(02-bmca-integration)
add_subdirectory(03-hal-implementation-template)

# Time Source Examples - External synchronization
add_subdirectory(04-gps-nmea-sync)             # GPS time source (NMEA + PPS)
add_subdirectory(05-ntp-sntp-sync)             # NTP time source (network)
add_subdirectory(06-dcf77-terrestrial-radio)   # DCF77 time source (radio, Europe)
add_subdirectory(07-rtc-module)                # RTC time source (I2C, fallback)
add_subdirectory(08-multi-source-bmca)         # Multi-source BMCA selection

# Hardware Integration Examples
add_subdirectory(12-intel-avb-windows)         # Intel AVB Filter Driver (Windows)

# Time-sensitive types example
add_executable(time_sensitive_types_example
    time_sensitive_types_example.cpp
)

target_link_libraries(time_sensitive_types_example
    IEEE1588_2019_interface
)

# Set C++14 for maximum compatibility
set_property(TARGET time_sensitive_types_example PROPERTY CXX_STANDARD 14)
set_property(TARGET time_sensitive_types_example PROPERTY CXX_STANDARD_REQUIRED ON)

# Compiler specific settings for deterministic behavior
if(MSVC)
    target_compile_options(time_sensitive_types_example PRIVATE 
        /W4 
        /wd4100  # Allow unreferenced parameters in examples
    )
else()
    target_compile_options(time_sensitive_types_example PRIVATE 
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter  # Allow unreferenced parameters in examples
    )
endif()

# Installation
install(TARGETS time_sensitive_types_example
    RUNTIME DESTINATION bin
)

install(FILES time_sensitive_types_example.cpp
    DESTINATION share/ieee_1588_2019/examples
)