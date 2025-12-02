# Installation Guide - IEEE 1588-2019 PTP v0.1.0

**Version**: 0.1.0 (Alpha Preview)  
**Release Date**: December 2, 2025  
**Status**: ⚠️ NOT PRODUCTION READY

---

## ⚠️ Important Notice

This is an **alpha/preview release**. Before installing:

- ✅ Use for evaluation, learning, and proof-of-concept only
- ❌ Do NOT use in production environments
- ❌ Do NOT use in safety-critical applications
- ⚠️ API may change without notice in future 0.x releases

See [Release Notes](./RELEASE_NOTES.md) for full details.

---

## System Requirements

### Supported Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| Linux (x86-64) | ✅ Tested | Ubuntu 20.04+, Debian 11+, RHEL 8+ |
| Windows (x86-64) | ✅ Tested | Windows 10+, Visual Studio 2019+ |
| macOS (x86-64, ARM64) | ⚠️ Expected to work | Not extensively tested |
| Embedded (ARM Cortex-M7) | 🔄 Target platform | Cross-compilation supported |

### Build Requirements

#### Minimum Requirements
- **CMake** ≥ 3.16
- **C++ Compiler** with C++14 support
  - GCC ≥ 7.0
  - Clang ≥ 6.0
  - MSVC ≥ 2019
- **C Compiler** with C11 support
- **Git** (for cloning repository)

#### Recommended for Development
- **CMake** ≥ 3.20 (latest features)
- **C++ Compiler** with C++17 support
- **Google Test** ≥ 1.10.0 (included as dependency)
- **Doxygen** ≥ 1.8.0 (for documentation generation)
- **clang-format** ≥ 10.0 (for code formatting)

#### Test Requirements
- **Google Test** (automatically fetched by CMake)
- **Python** ≥ 3.8 (for traceability scripts)

---

## Quick Start (5 Minutes)

### Linux / macOS

```bash
# 1. Clone repository
git clone https://github.com/zarfld/IEEE_1588_2019.git
cd IEEE_1588_2019

# 2. Create build directory
mkdir build && cd build

# 3. Configure with CMake
cmake ..

# 4. Build
cmake --build . -j$(nproc)

# 5. Run tests
ctest --output-on-failure

# Done! Library built at: build/lib/libieee1588_ptp.a
```

### Windows (PowerShell)

```powershell
# 1. Clone repository
git clone https://github.com/zarfld/IEEE_1588_2019.git
cd IEEE_1588_2019

# 2. Create build directory
mkdir build; cd build

# 3. Configure with CMake (Visual Studio)
cmake ..

# 4. Build (Release configuration)
cmake --build . --config Release

# 5. Run tests
ctest --output-on-failure -C Release

# Done! Library built at: build\lib\Release\ieee1588_ptp.lib
```

---

## Detailed Installation

### Step 1: Clone Repository

```bash
git clone https://github.com/zarfld/IEEE_1588_2019.git
cd IEEE_1588_2019
```

**Alternative: Download Release Archive**

```bash
wget https://github.com/zarfld/IEEE_1588_2019/archive/refs/tags/v0.1.0.tar.gz
tar -xzf v0.1.0.tar.gz
cd IEEE_1588_2019-0.1.0
```

### Step 2: Install Dependencies

#### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    doxygen
```

#### RHEL / CentOS / Fedora

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    python3 \
    python3-pip \
    doxygen
```

#### macOS (Homebrew)

```bash
brew install cmake python3 doxygen
```

#### Windows

- Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ support
- Install [CMake](https://cmake.org/download/) (add to PATH)
- Install [Git for Windows](https://git-scm.com/download/win)
- Install [Python 3.8+](https://www.python.org/downloads/) (optional, for scripts)

### Step 3: Configure Build

#### Basic Configuration

```bash
mkdir build && cd build
cmake ..
```

#### Advanced Configuration Options

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_TESTING=ON \
    -DBUILD_DOCUMENTATION=ON \
    -DCMAKE_CXX_STANDARD=17
```

**CMake Options**:

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Debug` | Build type: `Debug`, `Release`, `RelWithDebInfo` |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Installation directory |
| `BUILD_TESTING` | `ON` | Build test suite |
| `BUILD_DOCUMENTATION` | `OFF` | Generate Doxygen documentation |
| `CMAKE_CXX_STANDARD` | `14` | C++ standard: `14`, `17`, `20` |
| `CMAKE_C_STANDARD` | `11` | C standard: `11`, `17` |
| `ENABLE_COVERAGE` | `OFF` | Enable code coverage analysis |

### Step 4: Build

#### Build All Targets

```bash
# Linux/macOS
cmake --build . -j$(nproc)

# Windows
cmake --build . --config Release
```

#### Build Specific Targets

```bash
# Build library only
cmake --build . --target ieee1588_ptp

# Build tests only
cmake --build . --target tests

# Build documentation (if enabled)
cmake --build . --target docs
```

### Step 5: Run Tests

```bash
# Run all tests
ctest --output-on-failure

# Run with verbose output
ctest --output-on-failure --verbose

# Run specific test
ctest -R test_bmca --output-on-failure

# Run tests in parallel
ctest -j$(nproc)
```

**Expected Output** (v0.1.0):
```
Test project /path/to/build
    Start 1: test_clocks_api
1/88 Test #1: test_clocks_api ..................   Passed    0.01 sec
    ...
   88/88 Test #88: test_servo_convergence .........   Passed    0.23 sec

100% tests passed, 0 tests failed out of 88
```

### Step 6: Install (Optional)

```bash
# Install to CMAKE_INSTALL_PREFIX
sudo cmake --install .

# Or specify custom prefix
cmake --install . --prefix /opt/ieee1588_ptp
```

**Installed Files**:
```
<prefix>/
├── include/ieee1588_ptp/    # Public headers
├── lib/                     # Static/shared library
├── share/doc/               # Documentation
└── share/cmake/             # CMake package config
```

---

## Verification

After installation, verify the build:

### 1. Check Library Exists

```bash
# Linux/macOS
ls -lh build/lib/libieee1588_ptp.a

# Windows
dir build\lib\Release\ieee1588_ptp.lib
```

### 2. Run Test Suite

```bash
cd build
ctest --output-on-failure
```

**Expected**: 88/88 tests passing (100%)

### 3. Check Version

```bash
cat ../VERSION
# Expected output: 0.1.0
```

### 4. Verify Headers

```bash
# Check public API headers are accessible
ls ../include/ieee1588_ptp/
```

---

## Integration into Your Project

### Using CMake (Recommended)

#### Option 1: Find Package (After Installation)

```cmake
# In your CMakeLists.txt
find_package(IEEE1588_PTP REQUIRED)
target_link_libraries(your_target ieee1588::ptp)
```

#### Option 2: Add as Subdirectory

```cmake
# In your CMakeLists.txt
add_subdirectory(path/to/IEEE_1588_2019)
target_link_libraries(your_target ieee1588_ptp)
```

#### Option 3: FetchContent (CMake 3.14+)

```cmake
include(FetchContent)
FetchContent_Declare(
    ieee1588_ptp
    GIT_REPOSITORY https://github.com/zarfld/IEEE_1588_2019.git
    GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(ieee1588_ptp)
target_link_libraries(your_target ieee1588_ptp)
```

### Manual Integration

```bash
# Copy headers
cp -r include/ieee1588_ptp /your/project/include/

# Copy library
cp build/lib/libieee1588_ptp.a /your/project/lib/

# Link in your build system
g++ your_app.cpp -I/your/project/include -L/your/project/lib -lieee1588_ptp
```

---

## Cross-Compilation

### For ARM Embedded Targets

```bash
# Create toolchain file: arm-toolchain.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

# Configure with toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=../arm-toolchain.cmake
cmake --build .
```

### For Raspberry Pi

```bash
# Using Raspberry Pi toolchain
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/raspberry-pi.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

---

## Troubleshooting

### Common Issues

#### Issue: CMake version too old

```
Error: CMake 3.16 or higher is required
```

**Solution**: Update CMake:
```bash
# Ubuntu/Debian
sudo pip3 install --upgrade cmake

# Or download from cmake.org
wget https://cmake.org/files/v3.25/cmake-3.25.0-linux-x86_64.sh
sudo sh cmake-3.25.0-linux-x86_64.sh --prefix=/usr/local --skip-license
```

#### Issue: Google Test not found

```
Error: Could not find Google Test
```

**Solution**: CMake will automatically fetch Google Test. Ensure internet connection is available.

#### Issue: Compiler not found

```
Error: CMAKE_CXX_COMPILER not found
```

**Solution**: Install C++ compiler:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# RHEL/Fedora
sudo dnf install gcc-c++
```

#### Issue: Tests failing

```
Test failures: X/88 tests failed
```

**Solution**:
1. Check if running on supported platform
2. Ensure all dependencies are installed
3. Try clean rebuild: `rm -rf build && mkdir build && cd build && cmake .. && cmake --build .`
4. Report issue on GitHub with test output

### Getting Help

- **Documentation**: See `docs/` directory
- **Issues**: https://github.com/zarfld/IEEE_1588_2019/issues
- **Discussions**: https://github.com/zarfld/IEEE_1588_2019/discussions

---

## Next Steps

After successful installation:

1. **Read the Examples** - See `examples/` directory for usage patterns
2. **Review the Architecture** - `03-architecture/ieee-1588-2019-ptpv2-architecture-spec.md`
3. **Check the API** - Browse `include/ieee1588_ptp/` headers
4. **Try the Tests** - Study `tests/` for usage examples
5. **Integrate HAL** - See [Porting Guide](./PORTING_GUIDE.md)

---

## Uninstallation

```bash
# If installed via CMake
cd build
sudo cmake --build . --target uninstall

# Or manually remove
sudo rm -rf /usr/local/include/ieee1588_ptp
sudo rm -f /usr/local/lib/libieee1588_ptp.a
```

---

**Version**: 0.1.0  
**Last Updated**: December 2, 2025  
**Next Release**: v0.2.0 (Expected: January 2026)

For production use, please wait for v1.0.0 release (Target: May 2026).
