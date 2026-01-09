# Raspberry Pi Build and Test Guide

## Prerequisites on Raspberry Pi

### 1. Update System
```bash
sudo apt update
sudo apt upgrade -y
```

### 2. Install Build Tools
```bash
# Essential build tools
sudo apt install -y build-essential cmake git

# PTP and timing libraries
sudo apt install -y liblinuxptp-dev ethtool i2c-tools

# GPS libraries
sudo apt install -y gpsd gpsd-clients libgps-dev

# Network tools
sudo apt install -y net-tools wireshark-common tcpdump
```

### 3. Verify Hardware

```bash
# Check PTP Hardware Clock
ls -l /dev/ptp*
ethtool -T eth1

# Check GPS serial port
ls -l /dev/ttyACM0 /dev/ttyUSB0 /dev/ttyAMA0

# Check PPS device
ls -l /dev/pps0
cat /sys/class/pps/pps0/assert

# Check RTC
ls -l /dev/rtc*
sudo hwclock -r --rtc=/dev/rtc1
sudo i2cdetect -y 1  # Should show DS3231 at 0x68
```

## Build on Raspberry Pi

### Option 1: Clone and Build
```bash
# Clone repository
cd ~
git clone https://github.com/zarfld/IEEE_1588_2019.git
cd IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j4

# Verify binary
ls -lh ptp_grandmaster
./ptp_grandmaster --help
```

### Option 2: Build from Windows, Deploy to Pi
```powershell
# On Windows: Build for ARM64 (requires cross-compiler)
# Easier to build natively on Pi

# Alternatively, copy source and build on Pi
scp -r examples/12-RasPi5_i226-grandmaster pi@raspberrypi:~/
ssh pi@raspberrypi
cd ~/12-RasPi5_i226-grandmaster
mkdir build && cd build
cmake ..
make
```

## First Run: Device Detection

```bash
# Auto-detect devices
chmod +x scripts/detect-devices.sh
sudo ./scripts/detect-devices.sh

# Expected output:
# [PTP Hardware Clock (PHC)]
#   Found: /dev/ptp0
#   PTP Hardware Clock: 0
#   → Hardware timestamping supported
#
# [GPS Serial Devices]
#   Found: /dev/ttyACM0
#   $GPRMC,123456.00,A,4807.038,N,01131.000,E...
#   → NMEA GPS detected!
#
# [PPS (Pulse Per Second)]
#   Found: /dev/pps0
#   → PPS active
#
# [RTC (Real-Time Clock)]
#   Found: /dev/rtc1
#   Name: ds3231
#   → RTC accessible
#
# [Suggested Configuration]
# sudo ./ptp_grandmaster \
#     --interface=eth1 \
#     --phc=/dev/ptp0 \
#     --gps=/dev/ttyACM0 \
#     --pps=/dev/pps0 \
#     --rtc=/dev/rtc1 \
#     --verbose
```

## Test Run: GPS+PPS Integration

### 1. Stop Conflicting Services
```bash
# Stop services that interfere with GPS/PTP
sudo systemctl stop gpsd
sudo systemctl stop chronyd
sudo systemctl stop systemd-timesyncd

# Verify stopped
systemctl is-active gpsd chronyd systemd-timesyncd
```

### 2. Run Grandmaster with Verbose Output
```bash
cd ~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build

# Run with verbose logging
sudo ./ptp_grandmaster --verbose

# Or specify custom device paths
sudo ./ptp_grandmaster \
    --interface=eth1 \
    --phc=/dev/ptp0 \
    --gps=/dev/ttyACM0 \
    --pps=/dev/pps0 \
    --rtc=/dev/rtc1 \
    --verbose
```

### 3. Expected Console Output

**GPS Lock (within 60 seconds)**:
```
[GPS] Waiting for GPS fix...
[GPS] Fix acquired! Satellites: 8, HDOP: 1.2
[GPS] Position: 48.1234°N, 11.5678°E
[GPS] Time: 2025-01-09 14:23:45 UTC (TAI: 2025-01-09 14:24:22)
```

**PPS Debug (every 10 seconds)**:
```
[PPS] seq=10 time=14:23:45.000000000 jitter=150ns
[PPS] seq=20 time=14:23:55.000000000 jitter=120ns
[PPS] seq=30 time=14:24:05.000000000 jitter=95ns
```

**RTC Discipline Start (after 10 minutes)**:
```
[RTC Discipline] Starting drift measurement...
[RTC Discipline] GPS lock stable for 600 seconds, measuring drift over next 3600s
```

**RTC Discipline Complete (after 70 minutes total)**:
```
[RTC Discipline] Measured drift: -0.847 ppm
[RTC Discipline] Calculated aging offset: 8 LSB
[RTC Discipline] Current aging offset: 8
[RTC Discipline] RTC temperature: 24.5°C
```

**PTP Messages (every 1 second)**:
```
→ Announce message sent
→ Sync message sent
→ Follow_Up message sent
[PHC] Time set: 14:24:22.123456789
[RTC] Synced from GPS: 14:24:22.123456000
```

### 4. Monitor GPS+PPS Correlation

Watch for GPS time and PPS timestamp alignment:
```
[GPS+PPS] GPS second: 14:23:45, PPS second: 14:23:45 ✓ (aligned)
[GPS+PPS] GPS second: 14:23:46, PPS second: 14:23:46 ✓ (aligned)
[GPS+PPS] Using PPS nanosecond precision: 000000123
```

If misaligned:
```
[GPS+PPS] GPS second: 14:23:45, PPS second: 14:23:44 ✗ (>1s difference!)
[GPS+PPS] Falling back to GPS-only time
```

## Troubleshooting

### GPS Not Detected
```bash
# Check device exists
ls -l /dev/ttyACM0

# Check permissions
sudo chmod 666 /dev/ttyACM0

# Monitor GPS output manually
sudo cat /dev/ttyACM0 | grep -E '\$GP|\$GN'

# Check if gpsd is blocking
sudo systemctl stop gpsd
sudo killall gpsd
```

### PPS Not Working
```bash
# Verify PPS device exists
ls -l /dev/pps0

# Check kernel module
lsmod | grep pps
sudo modprobe pps-gpio

# Check Device Tree overlay
grep pps /boot/firmware/config.txt
# Should show: dtoverlay=pps-gpio,gpiopin=18

# Monitor PPS events
sudo ppstest /dev/pps0
```

### RTC Not Accessible
```bash
# Check I2C bus
sudo i2cdetect -y 1
# Should show device at 0x68 (DS3231)

# Test RTC manually
sudo hwclock -r --rtc=/dev/rtc1
sudo hwclock -w --rtc=/dev/rtc1

# Check Device Tree overlay
grep rtc /boot/firmware/config.txt
# Should show: dtoverlay=i2c-rtc,ds3231
```

### PHC Not Found
```bash
# Check network interface
ip link show eth1

# Check hardware timestamping capability
ethtool -T eth1
# Should show:
#   Capabilities:
#     hardware-transmit
#     hardware-receive
#   PTP Hardware Clock: 0

# Verify driver loaded
lsmod | grep i226
```

## Long-Term Testing

### 24-Hour Drift Measurement
```bash
# Run for 24 hours and log output
sudo ./ptp_grandmaster --verbose 2>&1 | tee gm_24h.log

# After 24 hours, analyze drift
grep "RTC Discipline" gm_24h.log
```

### PTP Client Testing
```bash
# On another device connected to eth1
sudo apt install linuxptp

# Run as PTP slave
sudo ptp4l -s -i eth0 -m

# Monitor synchronization
sudo pmc -u -b 0 'GET TIME_STATUS_NP'
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
```

### Capture PTP Traffic
```bash
# Capture PTP packets for Wireshark analysis
sudo tcpdump -i eth1 -nn ether proto 0x88f7 -w ptp_capture.pcap

# Analyze with Wireshark (on desktop)
scp pi@raspberrypi:~/ptp_capture.pcap .
wireshark ptp_capture.pcap
# Filter: ptp
```

## Performance Monitoring

### CPU and Memory Usage
```bash
# Monitor resources
htop

# Check CPU usage
top -p $(pgrep ptp_grandmaster)

# Memory usage
ps aux | grep ptp_grandmaster
```

### Network Statistics
```bash
# PTP packet counters
ethtool -S eth1 | grep ptp

# PHC status
sudo cat /sys/class/ptp/ptp0/pps_enable
```

### GPS Signal Quality
```bash
# Continuous monitoring
watch -n 5 'echo "Satellites visible:" && timeout 2 cat /dev/ttyACM0 | grep -o "GGA,[^,]*,[^,]*,[^,]*,[^,]*,[^,]*,[0-9]*" | tail -1'
```

## Installation as System Service

```bash
# Copy binary to system location
sudo cp ptp_grandmaster /usr/local/bin/

# Copy configuration file
sudo cp ../etc/ptp-grandmaster.conf /etc/

# Edit configuration if needed
sudo nano /etc/ptp-grandmaster.conf

# Copy systemd service file
sudo cp ../etc/systemd/ptp-grandmaster.service /etc/systemd/system/

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable ptp-grandmaster
sudo systemctl start ptp-grandmaster

# Check status
sudo systemctl status ptp-grandmaster

# Follow logs
sudo journalctl -u ptp-grandmaster -f
```

## Expected Timing Performance

| Metric | Expected Value | Notes |
|--------|----------------|-------|
| GPS Lock Time | < 60 seconds | Cold start with clear sky |
| GPS Accuracy | ±20 nanoseconds | With good PPS signal |
| PPS Jitter | < 200 nanoseconds | Measured between consecutive pulses |
| RTC Drift | ±2 ppm (factory) | DS3231 specification |
| RTC Drift (disciplined) | < 0.1 ppm | After aging offset applied |
| PTP Sync Accuracy | < 100 nanoseconds | With hardware timestamping |
| PHC Sync from GPS | < 50 nanoseconds | PPS-disciplined |

## Next Steps

1. ✅ **Verify GPS lock** - Wait for satellite fix (< 60 seconds)
2. ✅ **Check PPS integration** - Confirm jitter < 200ns
3. ⏳ **Wait 10 minutes** - GPS lock stabilization
4. ⏳ **Wait 70 minutes total** - RTC drift measurement completes
5. ⏳ **Verify aging offset** - Check console output for drift correction
6. ⏳ **Test GPS outage** - Disconnect GPS, verify RTC holdover
7. ⏳ **Connect PTP client** - Validate synchronization accuracy
8. ⏳ **24-hour stability test** - Long-term drift monitoring

---

**Hardware Requirements**:
- Raspberry Pi 5 (or compatible SBC)
- Intel i226 NIC (or PTP-capable NIC)
- GPS module with PPS output (u-blox, Adafruit, etc.)
- DS3231 RTC module on I2C
- Ethernet cable for PTP network

**Software Requirements**:
- Raspberry Pi OS 64-bit (Bookworm or later)
- Linux kernel 6.1+ with PPS and I2C support
- Device Tree overlays: `pps-gpio`, `i2c-rtc,ds3231`
