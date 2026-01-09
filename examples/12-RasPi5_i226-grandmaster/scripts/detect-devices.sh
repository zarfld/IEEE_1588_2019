#!/bin/bash
# GPS-Disciplined PTP Grandmaster - Device Auto-Detection
# Helps identify correct device paths for your hardware configuration

set -e

echo "=== GPS-Disciplined PTP Grandmaster Device Detection ==="
echo ""

# Detect PTP-capable network interfaces
echo "[PTP Hardware Clock (PHC)]"
if ls /dev/ptp* >/dev/null 2>&1; then
    for ptp in /dev/ptp*; do
        echo "  Found: $ptp"
        ethtool -T $(readlink /sys/class/ptp/$(basename $ptp)/device/net/* 2>/dev/null) 2>/dev/null | grep -E "PTP Hardware|Capabilities" || true
    done
else
    echo "  ⚠ No PTP devices found"
fi
echo ""

# Detect GPS serial devices
echo "[GPS Serial Devices]"
gps_found=false
for device in /dev/ttyACM* /dev/ttyUSB* /dev/ttyAMA* /dev/ttyS0; do
    if [ -e "$device" ]; then
        echo "  Found: $device"
        # Try to identify GPS
        timeout 2 cat "$device" 2>/dev/null | grep -m 1 -E '\$GP|\$GN' && echo "    → NMEA GPS detected!" || true
        gps_found=true
    fi
done
if ! $gps_found; then
    echo "  ⚠ No serial devices found"
fi
echo ""

# Detect PPS devices
echo "[PPS (Pulse Per Second)]"
if ls /dev/pps* >/dev/null 2>&1; then
    for pps in /dev/pps*; do
        echo "  Found: $pps"
        if [ -r "$pps" ]; then
            cat "/sys/class/pps/$(basename $pps)/assert" 2>/dev/null && echo "    → PPS active" || true
        fi
    done
else
    echo "  ⚠ No PPS devices found"
    echo "    Check: /boot/firmware/config.txt for pps-gpio overlay"
fi
echo ""

# Detect RTC devices
echo "[RTC (Real-Time Clock)]"
if ls /dev/rtc* >/dev/null 2>&1; then
    for rtc in /dev/rtc*; do
        echo "  Found: $rtc"
        rtc_name=$(cat "/sys/class/rtc/$(basename $rtc)/name" 2>/dev/null || echo "unknown")
        echo "    Name: $rtc_name"
        # Check if RTC is accessible
        hwclock -r --rtc="$rtc" >/dev/null 2>&1 && echo "    → RTC accessible" || echo "    ⚠ RTC not accessible"
    done
else
    echo "  ⚠ No RTC devices found"
fi
echo ""

# Generate suggested configuration
echo "[Suggested Configuration]"
phc=$(ls /dev/ptp* 2>/dev/null | head -1 || echo "/dev/ptp0")
gps=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1 || echo "/dev/ttyACM0")
pps=$(ls /dev/pps* 2>/dev/null | head -1 || echo "/dev/pps0")
rtc=$(ls /dev/rtc* 2>/dev/null | grep -v rtc0 | head -1 || echo "/dev/rtc1")

echo "sudo ./ptp_grandmaster \\"
echo "    --interface=eth1 \\"
echo "    --phc=$phc \\"
echo "    --gps=$gps \\"
echo "    --pps=$pps \\"
echo "    --rtc=$rtc \\"
echo "    --verbose"
echo ""

# Check for conflicting services
echo "[Service Conflicts]"
if systemctl is-active --quiet gpsd; then
    echo "  ⚠ gpsd is running and may interfere with GPS access"
    echo "    Suggestion: sudo systemctl stop gpsd"
fi
if systemctl is-active --quiet chronyd; then
    echo "  ⚠ chronyd is running and may conflict with RTC/PTP"
    echo "    Suggestion: sudo systemctl stop chronyd"
fi
if systemctl is-active --quiet systemd-timesyncd; then
    echo "  ⚠ systemd-timesyncd is running and may conflict with PTP"
    echo "    Suggestion: sudo systemctl stop systemd-timesyncd"
fi
echo ""

echo "=== Detection Complete ==="
