# Getting Started with IEEE 1588-2019 PTP Library

**Target Audience**: Developers new to the library  
**Time to Complete**: ~20 minutes  
**Difficulty**: Beginner  

This tutorial will help you get the IEEE 1588-2019 PTP library running on your system and create your first PTP-aware application.

---

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [5-Minute Quick Start](#5-minute-quick-start)
4. [Understanding the Library](#understanding-the-library)
5. [Your First PTP Application (15 Minutes)](#your-first-ptp-application)
6. [Running and Verifying](#running-and-verifying)
7. [Troubleshooting](#troubleshooting)
8. [Next Steps](#next-steps)

---

## Overview

The **IEEE 1588-2019 PTP Library** provides hardware-agnostic, standards-compliant Precision Time Protocol implementation. This library focuses on **pure protocol logic** - no OS-specific code, no hardware drivers - making it portable across:

- Embedded systems (ARM Cortex-M, RISC-V)
- Desktop applications (Windows, Linux, macOS)
- Real-time systems (RTOS, bare-metal)
- Data center applications

**Key Features**:
- ✅ **Hardware agnostic** - runs without physical PTP hardware
- ✅ **Deterministic** - no dynamic allocation in critical paths
- ✅ **Testable** - comprehensive test suite included
- ✅ **Standards compliant** - based on IEEE 1588-2019 specification

---

## Prerequisites

### Required Tools

| Tool | Minimum Version | Purpose | Installation |
|------|----------------|---------|--------------|
| **CMake** | 3.20+ | Build system | [cmake.org/download](https://cmake.org/download/) |
| **C++ Compiler** | C++17 | Build source | MSVC 2019+, GCC 7+, Clang 5+ |
| **Git** | 2.0+ | Clone repository | [git-scm.com](https://git-scm.com/) |
| **Python** | 3.7+ | Build scripts | [python.org](https://www.python.org/) |

### Platform-Specific Setup

#### Windows (PowerShell)
```powershell
# Verify installations
cmake --version       # Should show 3.20+
git --version        # Should show 2.0+
python --version     # Should show 3.7+
cl                   # Should show MSVC compiler (if using Visual Studio)
```

#### Linux (bash)
```bash
# Install prerequisites (Debian/Ubuntu)
sudo apt-get update
sudo apt-get install -y cmake g++ git python3

# Verify installations
cmake --version      # Should show 3.20+
g++ --version       # Should show 7.0+
git --version       # Should show 2.0+
python3 --version   # Should show 3.7+
```

#### macOS (Terminal)
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake (via Homebrew)
brew install cmake

# Verify installations
cmake --version      # Should show 3.20+
clang++ --version   # Should show 5.0+
git --version       # Should show 2.0+
python3 --version   # Should show 3.7+
```

---

## 5-Minute Quick Start

Get the library built and tested in under 5 minutes:

### Step 1: Clone Repository

```bash
# Clone the repository
git clone https://github.com/zarfld/IEEE_1588_2019.git
cd IEEE_1588_2019
```

### Step 2: Configure Build

**Windows (PowerShell)**:
```powershell
# Configure for Visual Studio
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Or configure for Ninja (faster)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

**Linux/macOS (bash)**:
```bash
# Configure with default generator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

### Step 3: Build

```bash
# Build all targets
cmake --build build --config Release
```

**Expected output**:
```
[100%] Built target ieee1588_2019_ptp
[100%] Built target basic_types_example
[100%] Built target simple_clock_test
Build succeeded
```

### Step 4: Run Tests

```bash
# Run all tests
ctest --test-dir build -C Release --output-on-failure
```

**Expected output**:
```
Test project D:/Repos/IEEE_1588_2019/build
    Start 1: TimestampTests
1/8 Test #1: TimestampTests ...................   Passed    0.01 sec
    Start 2: ClockTests
2/8 Test #2: ClockTests .......................   Passed    0.02 sec
...
100% tests passed, 0 tests failed out of 8
```

✅ **Success!** The library is built and all tests pass.

---

## Understanding the Library

Before writing code, let's understand the architecture:

### Core Components

```
┌─────────────────────────────────────────────────────┐
│           Your Application Layer                     │
│  (PTP Slave, Master, Boundary Clock, etc.)          │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│        IEEE 1588-2019 PTP Core Library              │
│  ┌──────────────┐  ┌──────────────┐                │
│  │   Message    │  │   Clock      │                │
│  │   Handling   │  │   State      │                │
│  └──────────────┘  │   Machine    │                │
│  ┌──────────────┐  └──────────────┘                │
│  │    BMCA      │  ┌──────────────┐                │
│  │  (Master     │  │   Data Sets  │                │
│  │  Selection)  │  │   (IEEE Sec  │                │
│  └──────────────┘  │    8)        │                │
│                    └──────────────┘                │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│         Hardware Abstraction Layer (HAL)            │
│  (You implement: network send/recv, timestamps)     │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│    Platform-Specific Code (OS, Drivers)            │
│  (Network stack, hardware timestamping, etc.)       │
└─────────────────────────────────────────────────────┘
```

### Key Concepts

1. **Hardware Agnostic Protocol Core**
   - Library contains **only IEEE 1588-2019 protocol logic**
   - No hardware drivers, no OS calls
   - You inject hardware access via interfaces

2. **Dependency Injection Pattern**
   - Library receives function pointers for hardware access
   - Your code implements: network send/receive, time capture, clock adjustment
   - Enables testing without real hardware

3. **Deterministic Design**
   - No dynamic memory allocation in critical paths
   - No blocking operations
   - Bounded execution time
   - Suitable for real-time systems

---

## Your First PTP Application

Let's create a simple PTP-aware application that demonstrates clock synchronization concepts.

### Step 1: Create Application File

Create a new file `my_first_ptp_app.cpp` in the `examples/` directory:

```cpp
/**
 * @file my_first_ptp_app.cpp
 * @brief My First PTP Application - Getting Started Tutorial
 * 
 * This example demonstrates basic PTP concepts:
 * - Creating a PTP clock identity
 * - Working with PTP timestamps
 * - Understanding clock quality
 */

#include <IEEE/1588/PTP/2019/ieee1588_2019.hpp>
#include <iostream>
#include <iomanip>
#include <ctime>

using namespace IEEE::_1588::PTP::_2019;

// Helper function to print clock identity
void print_clock_identity(const Types::ClockIdentity& id, const std::string& label) {
    std::cout << label << ": ";
    for (size_t i = 0; i < id.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned>(id[i]);
        if (i < id.size() - 1) std::cout << ":";
    }
    std::cout << std::dec << "\n";
}

// Helper function to print timestamp
void print_timestamp(const Types::Timestamp& ts, const std::string& label) {
    std::cout << label << ":\n";
    std::cout << "  Seconds:     " << ts.getTotalSeconds() << "\n";
    std::cout << "  Nanoseconds: " << ts.nanoseconds << "\n";
    std::cout << "  Valid:       " << (ts.isValid() ? "Yes" : "No") << "\n";
}

int main() {
    std::cout << "==============================================\n";
    std::cout << "  My First PTP Application\n";
    std::cout << "  IEEE 1588-2019 Getting Started Tutorial\n";
    std::cout << "==============================================\n\n";

    // Step 1: Create a Clock Identity
    // In real systems, this is typically derived from MAC address
    std::cout << "Step 1: Creating PTP Clock Identity\n";
    std::cout << "------------------------------------\n";
    
    Types::ClockIdentity my_clock_id = {
        0x00, 0x1B, 0x21, 0xFF, 0xFE, 0x12, 0x34, 0x56
    };
    
    print_clock_identity(my_clock_id, "My Clock ID");
    std::cout << "\n";

    // Step 2: Create a Port Identity
    std::cout << "Step 2: Creating PTP Port Identity\n";
    std::cout << "-----------------------------------\n";
    
    Types::PortIdentity my_port = {my_clock_id, 1};
    
    print_clock_identity(my_port.clock_identity, "Port's Clock ID");
    std::cout << "Port Number: " << my_port.port_number << "\n";
    std::cout << "\n";

    // Step 3: Work with PTP Timestamps
    std::cout << "Step 3: Working with PTP Timestamps\n";
    std::cout << "------------------------------------\n";
    
    // Create a timestamp representing "now"
    Types::Timestamp current_time;
    std::time_t now = std::time(nullptr);
    current_time.seconds_high = 0;
    current_time.seconds_low = static_cast<std::uint32_t>(now);
    current_time.nanoseconds = 123456789;  // Example nanoseconds
    
    print_timestamp(current_time, "Current PTP Time");
    std::cout << "\n";

    // Step 4: Create a Time Interval (e.g., 1.5 seconds)
    std::cout << "Step 4: Working with Time Intervals\n";
    std::cout << "------------------------------------\n";
    
    Types::TimeInterval interval;
    interval.scaled_nanoseconds = 1500000000LL << 16;  // 1.5 seconds in scaled format
    
    std::cout << "Time Interval: 1.5 seconds\n";
    std::cout << "Scaled Representation: " << interval.scaled_nanoseconds << "\n";
    std::cout << "\n";

    // Step 5: Clock Quality (used in BMCA)
    std::cout << "Step 5: Understanding Clock Quality\n";
    std::cout << "------------------------------------\n";
    
    Types::ClockQuality my_clock_quality{};
    my_clock_quality.clock_class = 248;        // Default class (application-specific)
    my_clock_quality.clock_accuracy = 0x21;    // Within 100ns
    my_clock_quality.offset_scaled_log_variance = 0x4E5D;  // Standard variance
    
    std::cout << "Clock Class:    " << static_cast<int>(my_clock_quality.clock_class) << "\n";
    std::cout << "Clock Accuracy: 0x" << std::hex << static_cast<int>(my_clock_quality.clock_accuracy) << std::dec << "\n";
    std::cout << "Log Variance:   0x" << std::hex << my_clock_quality.offset_scaled_log_variance << std::dec << "\n";
    std::cout << "\n";

    // Step 6: Correction Field (used for path delay)
    std::cout << "Step 6: Correction Field for Delays\n";
    std::cout << "------------------------------------\n";
    
    Types::CorrectionField path_delay;
    path_delay.correction_ns = 50000;  // 50 microseconds path delay
    
    std::cout << "Path Delay: " << path_delay.correction_ns << " nanoseconds\n";
    std::cout << "           = " << (path_delay.correction_ns / 1000.0) << " microseconds\n";
    std::cout << "\n";

    // Summary
    std::cout << "==============================================\n";
    std::cout << "  Tutorial Complete!\n";
    std::cout << "==============================================\n";
    std::cout << "\nYou've learned:\n";
    std::cout << "  ✓ How to create PTP clock and port identities\n";
    std::cout << "  ✓ How to work with PTP timestamps\n";
    std::cout << "  ✓ How to represent time intervals\n";
    std::cout << "  ✓ How clock quality affects master selection\n";
    std::cout << "  ✓ How correction fields account for delays\n";
    std::cout << "\nNext steps:\n";
    std::cout << "  → Check out examples/simple_clock_test.cpp\n";
    std::cout << "  → Read the Integration Guide (integration-guide.md)\n";
    std::cout << "  → Explore the API Reference (api-reference.md)\n";
    std::cout << "\n";

    return 0;
}
```

### Step 2: Add to CMake Build

Edit `examples/CMakeLists.txt` and add:

```cmake
# Add my first PTP app
add_executable(my_first_ptp_app my_first_ptp_app.cpp)
target_link_libraries(my_first_ptp_app PRIVATE ieee1588_2019_ptp)
target_include_directories(my_first_ptp_app PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)

# Install the example
install(TARGETS my_first_ptp_app DESTINATION bin/examples)
```

### Step 3: Build Your Application

```bash
# Reconfigure to pick up new file
cmake -S . -B build

# Build
cmake --build build --config Release --target my_first_ptp_app
```

---

## Running and Verifying

### Run Your Application

**Windows**:
```powershell
.\build\examples\Release\my_first_ptp_app.exe
```

**Linux/macOS**:
```bash
./build/examples/my_first_ptp_app
```

### Expected Output

```
==============================================
  My First PTP Application
  IEEE 1588-2019 Getting Started Tutorial
==============================================

Step 1: Creating PTP Clock Identity
------------------------------------
My Clock ID: 00:1b:21:ff:fe:12:34:56

Step 2: Creating PTP Port Identity
-----------------------------------
Port's Clock ID: 00:1b:21:ff:fe:12:34:56
Port Number: 1

Step 3: Working with PTP Timestamps
------------------------------------
Current PTP Time:
  Seconds:     1699564800
  Nanoseconds: 123456789
  Valid:       Yes

Step 4: Working with Time Intervals
------------------------------------
Time Interval: 1.5 seconds
Scaled Representation: 98304000000

Step 5: Understanding Clock Quality
------------------------------------
Clock Class:    248
Clock Accuracy: 0x21
Log Variance:   0x4e5d

Step 6: Correction Field for Delays
------------------------------------
Path Delay: 50000 nanoseconds
           = 50.0 microseconds

==============================================
  Tutorial Complete!
==============================================

You've learned:
  ✓ How to create PTP clock and port identities
  ✓ How to work with PTP timestamps
  ✓ How to represent time intervals
  ✓ How clock quality affects master selection
  ✓ How correction fields account for delays

Next steps:
  → Check out examples/simple_clock_test.cpp
  → Read the Integration Guide (integration-guide.md)
  → Explore the API Reference (api-reference.md)
```

✅ **Success!** You've created and run your first PTP-aware application.

---

## Troubleshooting

### Problem: CMake can't find the compiler

**Symptoms**:
```
CMake Error: CMake was unable to find a build program corresponding to "Ninja"
```

**Solution**:
```bash
# Windows: Use Visual Studio generator instead
cmake -S . -B build -G "Visual Studio 17 2022"

# Linux: Install build-essential
sudo apt-get install build-essential

# macOS: Install Xcode Command Line Tools
xcode-select --install
```

### Problem: Build fails with C++17 errors

**Symptoms**:
```
error: 'structured bindings' are not allowed in C++14
```

**Solution**:
```bash
# Explicitly set C++17 standard
cmake -S . -B build -DCMAKE_CXX_STANDARD=17
```

### Problem: Tests fail

**Symptoms**:
```
Test #1: TimestampTests ...................***Failed
```

**Solution**:
```bash
# Run with verbose output to see details
ctest --test-dir build -C Release --output-on-failure --verbose

# Check if it's a path issue (Windows)
# Make sure build directory is on the same drive as source
```

### Problem: Can't find header files

**Symptoms**:
```cpp
fatal error: IEEE/1588/PTP/2019/ieee1588_2019.hpp: No such file
```

**Solution**:
```cmake
# Make sure your CMakeLists.txt includes proper paths
target_include_directories(your_target PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)
```

### Problem: Link errors

**Symptoms**:
```
undefined reference to `IEEE::_1588::PTP::_2019::Types::Timestamp::isValid()'
```

**Solution**:
```cmake
# Make sure you link the library
target_link_libraries(your_target PRIVATE ieee1588_2019_ptp)
```

### Getting Help

If you encounter issues not covered here:

1. **Check existing examples**: Look at `examples/basic_types_example.cpp` and `examples/simple_clock_test.cpp`
2. **Review documentation**: See `integration-guide.md` for detailed integration steps
3. **Check GitHub Issues**: Search for similar problems at https://github.com/zarfld/IEEE_1588_2019/issues
4. **Open an issue**: If you found a bug, please report it with:
   - Your operating system and version
   - Compiler and version
   - CMake version
   - Full error message
   - Steps to reproduce

---

## Next Steps

Congratulations! You've completed the Getting Started tutorial. Here's what to explore next:

### 1. Study More Examples

**Basic Examples** (`examples/` directory):
- `basic_types_example.cpp` - Demonstrates all core PTP types
- `simple_clock_test.cpp` - Shows clock state machine usage
- `time_sensitive_types_example.cpp` - Real-time data types

### 2. Read Documentation

**Essential Documentation**:
- **API Reference** (`api-reference.md`) - Complete API documentation with examples
- **Integration Guide** (`integration-guide.md`) - How to integrate into your product
- **Doxygen Documentation** - Generated API docs (run `py scripts\generate-doxygen.py`)

### 3. Implement HAL Layer

The library needs hardware abstraction implementations:

```cpp
// Your implementation of network interface
struct MyNetworkInterface {
    int send_packet(const void* data, size_t length) {
        // Your code to send PTP packet over network
        return 0;  // Return 0 on success
    }
    
    uint64_t get_timestamp_ns() {
        // Your code to get current time in nanoseconds
        return 0;
    }
};
```

See `integration-guide.md` Section 3 "HAL Implementation" for complete details.

### 4. Build a PTP Slave

Create a working PTP slave clock:

1. Implement network send/receive
2. Implement timestamp capture
3. Create clock adjustment mechanism
4. Connect to a PTP master
5. Synchronize your clock

Full example coming soon in `examples/ptp_slave_complete/`

### 5. Explore Advanced Topics

- **BMCA (Best Master Clock Algorithm)** - How masters are selected
- **Delay Mechanisms** - End-to-end vs peer-to-peer
- **Transparent Clocks** - Switching with correction
- **Boundary Clocks** - Multi-domain synchronization
- **IEEE 802.1AS (gPTP)** - Profile for AVB/TSN networks

### 6. Contribute

Help improve the library:

1. Report bugs or suggest features
2. Add new examples
3. Improve documentation
4. Submit pull requests

See `CONTRIBUTING.md` for guidelines.

---

## Quick Reference

### Build Commands

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all
cmake --build build --config Release

# Build specific target
cmake --build build --config Release --target my_first_ptp_app

# Run tests
ctest --test-dir build -C Release

# Install (optional)
cmake --install build --prefix /path/to/install
```

### Key Files to Know

| File | Purpose |
|------|---------|
| `include/IEEE/1588/PTP/2019/*.hpp` | Public API headers |
| `src/*.cpp` | Implementation source |
| `examples/*.cpp` | Example applications |
| `tests/*.cpp` | Test suite |
| `CMakeLists.txt` | Build configuration |
| `api-reference.md` | API documentation |
| `integration-guide.md` | Integration instructions |

### Important Namespaces

```cpp
namespace IEEE {
    namespace _1588 {
        namespace PTP {
            namespace _2019 {
                namespace Types { /* Core PTP types */ }
                namespace Clocks { /* Clock state machines */ }
                namespace Messages { /* PTP message formats */ }
                namespace BMCA { /* Master selection */ }
            }
        }
    }
}
```

---

## Feedback

We'd love to hear about your experience with this tutorial!

- **Was it helpful?** Let us know what worked well
- **Got stuck?** Tell us where you had trouble
- **Suggestions?** We're always looking to improve

Open an issue on GitHub or send feedback to the maintainers.

**Happy PTP coding!** 🚀⏱️
