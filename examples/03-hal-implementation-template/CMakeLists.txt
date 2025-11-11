# Example 3: HAL Implementation Template
# Platform porting guide and template files
# This is a template/documentation example, not a compilable program

cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Platform Detection
# ============================================================================

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_LINUX TRUE)
    message(STATUS "Detected Linux platform")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_WINDOWS TRUE)
    message(STATUS "Detected Windows platform")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeRTOS")
    set(PLATFORM_FREERTOS TRUE)
    message(STATUS "Detected FreeRTOS platform")
else()
    message(STATUS "Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif()

# ============================================================================
# Optional: Build HAL Template Example
# ============================================================================

# This option allows users to build an example using the HAL template
# ONLY if they have implemented the HAL functions for their platform
option(BUILD_HAL_TEMPLATE_EXAMPLE "Build HAL template example (requires platform-specific implementation)" OFF)

if(BUILD_HAL_TEMPLATE_EXAMPLE)
    message(STATUS "Building HAL template example")
    
    # Create executable from template
    add_executable(hal_template_example
        hal_template.cpp
        # Add your platform-specific source files here:
        # ${PLATFORM_SPECIFIC_SOURCES}
    )
    
    # Link with PTP library
    target_link_libraries(hal_template_example PRIVATE
        ieee1588_2019_ptp
    )
    
    # Platform-specific libraries
    if(PLATFORM_LINUX)
        # Link with pthread and real-time library if needed
        target_link_libraries(hal_template_example PRIVATE pthread rt)
    elseif(PLATFORM_WINDOWS)
        # Link with Winsock2 for network operations
        target_link_libraries(hal_template_example PRIVATE ws2_32)
    elseif(PLATFORM_FREERTOS)
        # Add FreeRTOS-specific libraries
        # target_link_libraries(hal_template_example PRIVATE freertos lwip)
    endif()
    
    # C++ standard
    set_target_properties(hal_template_example PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Compiler warnings
    if(MSVC)
        target_compile_options(hal_template_example PRIVATE /W4)
    else()
        target_compile_options(hal_template_example PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    
    # Install binary
    install(TARGETS hal_template_example
        RUNTIME DESTINATION bin
        COMPONENT examples
    )
else()
    message(STATUS "HAL template example not enabled. Set BUILD_HAL_TEMPLATE_EXAMPLE=ON to build.")
    message(STATUS "Note: You must implement the HAL functions for your platform first.")
endif()

# ============================================================================
# Install Template Files
# ============================================================================

# Install all template files for users to copy and adapt
install(FILES
    README.md
    hal_template.hpp
    hal_template.cpp
    porting_checklist.md
    DESTINATION share/ieee1588_2019_ptp/examples/03-hal-implementation-template
    COMPONENT examples
)

message(STATUS "HAL template files will be installed to share/ieee1588_2019_ptp/examples/03-hal-implementation-template")
