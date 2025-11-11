# Example 1: Basic PTP Slave
# Demonstrates complete PTP slave synchronization sequence

# Minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Add executable
add_executable(basic_ptp_slave
    basic_ptp_slave.cpp
    minimal_hal.cpp
)

# Link to PTP library
target_link_libraries(basic_ptp_slave PRIVATE
    ieee1588_2019_ptp
)

# Include directories
target_include_directories(basic_ptp_slave PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++ standard
set_target_properties(basic_ptp_slave PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Compiler warnings
if(MSVC)
    target_compile_options(basic_ptp_slave PRIVATE /W4)
else()
    target_compile_options(basic_ptp_slave PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS basic_ptp_slave
    DESTINATION bin/examples
    COMPONENT examples
)

# Install supporting files
install(FILES
    README.md
    minimal_hal.hpp
    minimal_hal.cpp
    basic_ptp_slave.cpp
    DESTINATION share/ieee1588_2019_ptp/examples/01-basic-ptp-slave
    COMPONENT examples
)
