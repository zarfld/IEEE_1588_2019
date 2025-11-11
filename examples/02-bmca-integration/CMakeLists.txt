# Example 2: BMCA Integration
# Demonstrates Best Master Clock Algorithm with multiple competing clocks

# Minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Add executable
add_executable(bmca_integration
    bmca_integration.cpp
)

# Link to PTP library
target_link_libraries(bmca_integration PRIVATE
    ieee1588_2019_ptp
)

# Include directories
target_include_directories(bmca_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++ standard
set_target_properties(bmca_integration PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Compiler warnings
if(MSVC)
    target_compile_options(bmca_integration PRIVATE /W4)
else()
    target_compile_options(bmca_integration PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS bmca_integration
    DESTINATION bin/examples
    COMPONENT examples
)

# Install supporting files
install(FILES
    README.md
    bmca_integration.cpp
    DESTINATION share/ieee1588_2019_ptp/examples/02-bmca-integration
    COMPONENT examples
)
