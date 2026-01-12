#!/bin/bash
#
# Configure DS3231 RTC 1Hz Square Wave Output
# 
# This script temporarily unbinds the kernel RTC driver to configure
# the SQW output, then rebinds it. Required because the kernel driver
# maintains exclusive I2C access to the DS3231.
#
# Hardware: DS3231 RTC on I2C bus 15, address 0x68, SQW pin → GPIO22 (/dev/pps1)

set -e  # Exit on error

echo "=== DS3231 SQW Configuration Script ==="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "ERROR: This script must be run as root (sudo)"
    exit 1
fi

# Configuration
I2C_BUS="15"  # DS3231 on I2C bus 15 (i2c-gpio-rtc)
I2C_ADDR="0x68"
CONTROL_REG="0x0E"
SQW_VALUE="0x00"  # INTCN=0 (SQW enabled), RS=00 (1Hz)

echo "Step 1: Finding DS3231 RTC device in kernel..."

# Look for DS3231 specifically on I2C bus 14, address 0x68
# The device name format is "14-0068"
EXPECTED_I2C_DEVICE="$I2C_BUS-0068"

# Search all RTC devices for the one matching our I2C bus and address
RTC_DEVICE=""
for rtc in /sys/class/rtc/rtc*; do
    if [ -e "$rtc/device" ]; then
        device_path=$(readlink -f $rtc/device)
        device_name=$(basename $device_path)
        echo "  Checking: $(basename $rtc) → $device_name"
        
        if [ "$device_name" = "$EXPECTED_I2C_DEVICE" ]; then
            RTC_DEVICE=$(basename $rtc)
            echo "  ✓ Found DS3231: /dev/$RTC_DEVICE"
            break
        fi
    fi
done

if [ -z "$RTC_DEVICE" ]; then
    echo "ERROR: DS3231 RTC not found at I2C address $I2C_BUS-0068"
    echo "Available RTC devices:"
    for rtc in /sys/class/rtc/rtc*; do
        if [ -e "$rtc/device" ]; then
            device_path=$(readlink -f $rtc/device)
            device_name=$(basename $device_path)
            echo "  $(basename $rtc) → $device_name"
        fi
    done
    echo ""
    echo "Hint: Check your config.txt i2c-rtc-gpio configuration"
    echo "      Expected: dtoverlay=i2c-rtc-gpio,ds3231,addr=0x68,..."
    exit 1
fi

# Find the I2C device path
I2C_DEVICE_PATH=$(readlink -f /sys/class/rtc/$RTC_DEVICE/device)
echo "  Kernel device: $I2C_DEVICE_PATH"

# Extract the device name (e.g., "14-0068")
DEVICE_NAME=$(basename $I2C_DEVICE_PATH)
echo "  Device name: $DEVICE_NAME"

# Find the driver being used
DRIVER_PATH=$(readlink -f $I2C_DEVICE_PATH/driver)
DRIVER_NAME=$(basename $DRIVER_PATH)
echo "  Driver: $DRIVER_NAME"

echo ""
echo "Step 2: Unbinding kernel RTC driver..."
echo "  This temporarily disables /dev/$RTC_DEVICE"
echo $DEVICE_NAME > /sys/bus/i2c/drivers/$DRIVER_NAME/unbind
echo "  ✓ Driver unbound"

# Give the I2C bus a moment to settle
sleep 0.5

echo ""
echo "Step 3: Configuring DS3231 control register..."
echo "  I2C Bus: $I2C_BUS"
echo "  Address: $I2C_ADDR"
echo "  Control Register: $CONTROL_REG"
echo "  Value: $SQW_VALUE (1Hz square wave)"

# Try to read current value first
echo -n "  Current value: "
CURRENT=$(i2cget -y $I2C_BUS $I2C_ADDR $CONTROL_REG 2>/dev/null) || CURRENT="N/A"
echo "$CURRENT"

# Write the SQW configuration
if i2cset -y $I2C_BUS $I2C_ADDR $CONTROL_REG $SQW_VALUE; then
    echo "  ✓ Control register written"
    
    # Verify
    sleep 0.1
    VERIFY=$(i2cget -y $I2C_BUS $I2C_ADDR $CONTROL_REG 2>/dev/null) || VERIFY="N/A"
    if [ "$VERIFY" = "$SQW_VALUE" ]; then
        echo "  ✓ Verified: $VERIFY"
    else
        echo "  ⚠ Verification mismatch: wrote $SQW_VALUE, read $VERIFY"
    fi
else
    echo "  ERROR: Failed to write control register"
    echo ""
    echo "Attempting to rebind driver anyway..."
fi

echo ""
echo "Step 4: Rebinding kernel RTC driver..."
echo $DEVICE_NAME > /sys/bus/i2c/drivers/$DRIVER_NAME/bind
echo "  ✓ Driver rebound"
echo "  ✓ /dev/$RTC_DEVICE is now available again"

echo ""
echo "Step 5: Checking SQW output on /dev/pps1..."
if [ -e /dev/pps1 ]; then
    echo "  /dev/pps1 exists"
    echo "  Testing for PPS pulses (10 second timeout)..."
    
    # Test for 10 seconds
    timeout 10 ppstest /dev/pps1 2>&1 | head -5 &
    PPS_PID=$!
    sleep 2
    
    if ps -p $PPS_PID > /dev/null 2>&1; then
        echo "  ✓ PPS pulses detected on /dev/pps1"
        kill $PPS_PID 2>/dev/null || true
    else
        echo "  ⚠ No PPS pulses detected (check GPIO22 connection)"
    fi
else
    echo "  ⚠ /dev/pps1 does not exist"
    echo "  Check device tree configuration in /boot/firmware/config.txt:"
    echo "    dtoverlay=pps-gpio,gpiopin=22,pull=down,name=rtc-sqw"
fi

echo ""
echo "=== Configuration Complete ==="
echo ""
echo "The DS3231 should now output 1Hz square wave on GPIO22."
echo "This configuration persists until the DS3231 loses power."
echo ""
echo "To verify:"
echo "  sudo ppstest /dev/pps1"
echo ""
echo "Now you can run the PTP grandmaster:"
echo "  sudo ./ptp_grandmaster --interface=eth1 --rtc-sqw=/dev/pps1"
echo ""
