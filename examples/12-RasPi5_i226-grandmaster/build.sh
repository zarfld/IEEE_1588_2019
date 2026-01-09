#!/bin/bash
# GPS-Disciplined PTP Grandmaster - Build Script
# Raspberry Pi 5 + Intel i226 NIC

set -e

echo "=== GPS-Disciplined PTP Grandmaster Build Script ==="
echo ""

# Check if running on Raspberry Pi
if [ ! -f /proc/device-tree/model ]; then
    echo "⚠️  Warning: Not running on Raspberry Pi"
    echo "   This build is optimized for Raspberry Pi 5 ARM64"
fi

# Check for required tools
echo "[1/6] Checking build dependencies..."
MISSING_DEPS=()

for cmd in cmake make g++ gcc pkg-config; do
    if ! command -v $cmd &> /dev/null; then
        MISSING_DEPS+=($cmd)
    fi
done

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "❌ Missing dependencies: ${MISSING_DEPS[*]}"
    echo "   Install with: sudo apt install build-essential cmake"
    exit 1
fi
echo "✅ Build tools found"

# Check for PTP libraries
echo "[2/6] Checking PTP libraries..."
if [ ! -f /usr/include/linux/ptp_clock.h ]; then
    echo "⚠️  Warning: PTP headers not found"
    echo "   Install with: sudo apt install liblinuxptp-dev"
fi

# Create build directory
echo "[3/6] Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "[4/6] Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_C_COMPILER=gcc \
      -DCMAKE_VERBOSE_MAKEFILE=OFF \
      ..

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    exit 1
fi
echo "✅ CMake configuration successful"

# Build
echo "[5/6] Building..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi
echo "✅ Build successful"

# Verify binary
echo "[6/6] Verifying binary..."
if [ -x ptp_grandmaster ]; then
    echo "✅ Binary created: build/ptp_grandmaster"
    ls -lh ptp_grandmaster
    echo ""
    echo "=== Build Complete ==="
    echo ""
    echo "Next steps:"
    echo "  1. Check hardware: sudo ../scripts/detect-devices.sh"
    echo "  2. Run grandmaster: sudo ./ptp_grandmaster --verbose"
    echo "  3. See RASPBERRY_PI_BUILD.md for full testing guide"
else
    echo "❌ Binary not found"
    exit 1
fi
