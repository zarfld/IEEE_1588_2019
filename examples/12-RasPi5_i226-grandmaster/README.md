# Raspberry Pi 5 + Intel i226 PTP Grandmaster Clock

## 🎯 Project Overview

High-precision **IEEE 1588-2019 PTP Grandmaster Clock** using Raspberry Pi 5 with:
- **Intel i226 Ethernet** (PCIe) - Hardware PTP timestamping support
- **u-blox GPS Module** - Primary time reference (NMEA + 1PPS)
- **DS3231 RTC** - Holdover during GPS outages
- **GPIO PPS Input** - Hardware-disciplined timing
- **This Repository's Code** - IEEE 1588-2019 compliant implementation

**Result**: High-accuracy GPS-disciplined PTP grandmaster for network time distribution!

---

## 📊 Current System Status

Based on [console.log](console.log) analysis:

### ✅ Hardware Verified Working
- ✅ **PPS Signal**: `/dev/pps0` receiving stable 1Hz pulses from GPS
  - Jitter: < 2 µs (1767969261.003771834 → 1767969265.003773106)
  - Sequence: 72054-72058 (continuous, no drops)
- ✅ **Intel i226 NIC**: PCIe detected at `0001:01:00.0`
  - Device ID: `[8086:125c]` (rev 04)
  - Interface: `eth1` (MAC: `1c:fd:08:7e:21:b1`)
  - State: DOWN (cable not connected yet)
- ✅ **PTP Hardware Clocks**:
  - `/dev/ptp0` → eth1 (i226 hardware clock)
  - `/dev/ptp1` → gem-ptp-timer (SoC timer)
- ✅ **GPS Module**: u-blox G70xx on `/dev/ttyACM0`
  - Chipset: UBX-G70xx (firmware PROTVER 14.00)
  - Antenna: OK
  - NMEA output: Working (38400 baud, pure NMEA mode)
  - GPS Time: TAI with nanosecond precision
  - Fix Quality: 1 (GPS), 7-9 satellites visible
- ✅ **DS3231 RTC**: Temperature-compensated crystal oscillator
  - Device: `/dev/rtc1` (kernel driver rtc-ds1307)
  - I2C Bus: Bus 14 at address 0x68 (confirmed via i2cdetect)
  - Aging Offset: Register 0x10 accessible (±12.7 ppm range, 0.1 ppm per LSB)
  - Drift Measurement: Automated 60-sample buffer (~0.2-0.3 ppm typical)
- ✅ **LinuxPTP**: v4.2 installed
  - `ptp4l`, `phc2sys`, `ts2phc` available
- ✅ **Chrony**: Configured with GPS+PPS
  - SHM 0: GPS time via NMEA
  - PPS: `/dev/pps0` locked to GPS
  - RTC sync enabled

### 🔧 Implementation Status

**✅ COMPLETED**:
- ✅ **GPS Adapter** (`gps_adapter.cpp`):
  - NMEA parser (GPRMC + GPGGA sentences) with manual CSV parsing
  - UTC to TAI conversion (37-second leap second offset)
  - PPS signal integration with nanosecond precision
  - PPS jitter measurement (0.6-3.8µs typical)
  - GPS fix quality monitoring
  
- ✅ **RTC Adapter** (`rtc_adapter.cpp`):
  - DS3231 I2C interface (I2C bus 14 at address 0x68)
  - Aging offset read/write (register 0x10)
  - Automated drift measurement (60-sample buffer)
  - Frequency discipline (±12.7 ppm range)
  - Temperature sensor reading
  - Code consolidation: single I2C write point (no race conditions)
  
- ✅ **PTP Grandmaster** (`ptp_grandmaster.cpp`):
  - IEEE 1588-2019 message construction (Announce, Sync, Follow_Up)
  - GPS-disciplined clock quality (Class=7, Accuracy=33 for <100ns)
  - Hardware timestamp integration
  - Event loop with GPS/RTC fallback
  - Compilation verified on Raspberry Pi
  
- ✅ **Linux PTP HAL** (`linux_ptp_hal.cpp`):
  - Socket operations (event/general)
  - Hardware timestamping (SO_TIMESTAMPING)
  - PHC clock operations

**⏳ READY FOR TESTING**:
- [ ] **eth1 UP** - Connect network cable
- [ ] **RTC aging offset I2C writes** - Test on correct bus 14 (errno=121 fixed)
- [ ] **PTP network transmission** - Verify packets with tcpdump/Wireshark
- [ ] **Slave synchronization** - Test with PTP slave device

**Current Status**: GPS integration ✅, RTC discipline implemented ✅, PTP messages compiled ✅, I2C bus corrected (13→14) ✅

---

## 🎯 Integration Plan

### Phase 1: Baseline Validation (Current State)
**Objective**: Verify existing LinuxPTP grandmaster functionality

1. **Connect Network Cable** to eth1
   ```bash
   sudo ip link set eth1 up
   ```

2. **Test Standard LinuxPTP Grandmaster**
   ```bash
   # Start PTP grandmaster on eth1
   sudo systemctl start ptp4l-gm.service
   sudo systemctl status ptp4l-gm.service
   
   # Sync PHC to system clock (GPS-disciplined)
   sudo systemctl start phc2sys-ptp0.service
   sudo systemctl status phc2sys-ptp0.service
   ```

3. **Verify PTP Grandmaster**
   ```bash
   # Check PTP state
   sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
   sudo pmc -u -b 0 'GET TIME_STATUS_NP'
   
   # Monitor synchronization
   sudo journalctl -u ptp4l-gm.service -f
   ```

**Success Criteria**:
- ✅ `ptp4l` achieves MASTER state
- ✅ Announce messages transmitted on eth1
- ✅ PHC clock within ±1µs of GPS time
- ✅ Slave devices can synchronize

### Phase 2: Repository Implementation ✅ COMPLETE
**Objective**: IEEE 1588-2019 PTP Grandmaster with GPS+RTC discipline
**Status**: Implementation complete, ready for integration testing

#### 2.1 Build and Run Application

```bash
# On Raspberry Pi (or development machine with cross-compile)
cd ~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster

# Build the application
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Run the PTP grandmaster
sudo ./ptp_grandmaster --interface=eth1 --gps=/dev/ttyACM0 --pps=/dev/pps0 --rtc=/dev/rtc1

# Or install system-wide
sudo make install
sudo /usr/local/bin/ptp_grandmaster --interface=eth1
```

#### 2.2 Implementation Details

**GPS Adapter** (`src/gps_adapter.cpp`):
- Opens serial port `/dev/ttyACM0` with auto-baud detection (38400 baud)
- Disables u-blox UBX binary protocol (forces pure NMEA mode)
- Parses GPRMC sentences (UTC time + date) with manual CSV parser for empty field handling
- Parses GPGGA sentences (fix quality + satellite count)
- Correlates PPS pulses with NMEA time for nanosecond precision
- Converts UTC to TAI (37-second GPS leap second offset)
- Tracks PPS jitter and sequence numbers

**RTC Adapter** (`src/rtc_adapter.cpp`):
- Opens `/dev/rtc1` (kernel driver rtc-ds1307) and `/dev/i2c-14` (DS3231 hardware)
- Uses I2C_SLAVE_FORCE to access aging offset register while kernel driver active
- Implements 60-sample drift measurement buffer
- Calculates aging offset corrections (1 LSB = 0.1 ppm)
- Single I2C write point (consolidated code, no race conditions)
- Reads DS3231 temperature sensor
- Automated 1-hour drift measurement after 10 minutes of GPS lock

**Linux PTP HAL** (`src/linux_ptp_hal.cpp`):
- Socket creation and binding (event/general)
- Hardware timestamping (SO_TIMESTAMPING)
- PTP message send/receive
- PHC clock operations (clock_gettime/clock_settime)

#### 2.3 PTP Grandmaster Application

**File**: `src/ptp_grandmaster.cpp` ✅ **IMPLEMENTED**

**Key Features**:
- **Command-line Options**: `--interface`, `--phc`, `--gps`, `--pps`, `--rtc`, `--verbose`
- **GPS Integration**: Continuous GPS NMEA parsing with PPS correlation
- **Clock Quality**: Class 7 (GPS-disciplined), Accuracy 33 (<100ns)
- **Message Transmission**:
  - Announce messages every 2 seconds
  - Sync messages every 1 second
  - Follow_Up messages with hardware TX timestamps
- **RTC Discipline**: Automated drift measurement and aging offset correction
- **Error Handling**: Graceful GPS loss fallback to RTC holdover

**Actual Implementation** (522 lines):
```cpp
// Main event loop (simplified)
while (g_running) {
    // Update GPS data
    gps_adapter.update();
    
    // Check GPS fix status
    if (gps_adapter.has_fix()) {
        uint64_t gps_seconds, gps_nanos;
        gps_adapter.get_ptp_time(&gps_seconds, &gps_nanos);
        
        // Update clock quality based on GPS
        clock_class = 7;  // GPS-disciplined
        clock_accuracy = 33;  // <100ns
        
        // Measure RTC drift (automated after 10 min GPS lock)
        if (drift_measurement_start_time > 0) {
            // Calculate and apply aging offset correction
            rtc_adapter.apply_frequency_discipline(drift_ppm);
        }
        
        // Send PTP messages
        send_announce_message();  // Every 2 seconds
        send_sync_message();      // Every 1 second with HW timestamps
    }
}
```

**Message Construction**:
- Uses IEEE 1588-2019 types from repository (`AnnounceMessage`, `SyncMessage`, `FollowUpMessage`)
- Correct field names: `clock_identity`, `port_number` (snake_case)
- Timestamp methods: `setTotalSeconds()`, `nanoseconds` field
- ClockIdentity handling: `std::array` with `.data()` method

#### 2.4 Testing and Verification

**Test on Raspberry Pi**:
```bash
# Build and run
cd ~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build
sudo ./ptp_grandmaster --interface=eth1 --verbose

# Expected output:
# [GPS] GPS fix acquired, 7 satellites
# [GPS Time] 1767983525.218112 TAI
# [PPS] Jitter: 1.234 µs
# [RTC Discipline] ✓ Aging offset applied: -2 LSB
# [PTP] Sending Announce message...
# [PTP] Sending Sync message...
# [PTP] Follow_Up sent with TX timestamp
```

**Verify PTP Packets**:
```bash
# On another machine or Raspberry Pi:
sudo tcpdump -i eth1 -nn 'multicast and udp port 319' -vv
# Should see Announce (every 2s), Sync+Follow_Up (every 1s)

# Or use Wireshark:
sudo wireshark -i eth1 -f "udp port 319 or udp port 320"
# Filter: ptp
```

### Phase 3: Remote Debugging Setup

#### 3.1 GDB Remote Debugging

**On Raspberry Pi** (target):
```bash
# Install gdbserver
sudo apt install gdbserver

# Start application under gdbserver
gdbserver :2345 /usr/local/bin/ptp_grandmaster --interface=eth1
```

**On Development Machine** (host):
```bash
# Install GDB with ARM64 support
sudo apt install gdb-multiarch

# Connect to target
gdb-multiarch /path/to/ptp_grandmaster
(gdb) target remote 192.168.x.x:2345
(gdb) break main
(gdb) continue
```

#### 3.2 VS Code Remote Development

**Install VS Code Extensions**:
- Remote - SSH
- C/C++ Extension Pack
- CMake Tools

**.vscode/launch.json** (Remote Debug Configuration):
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Remote GDB Debug",
            "type": "cppdbg",
            "request": "launch",
            "program": "/usr/local/bin/ptp_grandmaster",
            "args": ["--interface=eth1", "--gps=/dev/ttyACM0"],
            "stopAtEntry": false,
            "cwd": "/home/zarfld",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "miDebuggerPath": "/usr/bin/gdb",
            "miDebuggerServerAddress": "localhost:2345",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}
```

#### 3.3 Performance Profiling

```bash
# Install profiling tools
sudo apt install linux-perf

# Profile PTP application
sudo perf record -F 999 -g /usr/local/bin/ptp_grandmaster
sudo perf report

# Monitor real-time latency
sudo cyclictest -p 80 -t 1 -n -i 1000 -l 100000
```

---

## 🔍 Monitoring & Diagnostics

### PTP Status Monitoring
```bash
# Repository implementation status
/usr/local/bin/ptp_grandmaster --status

# Standard LinuxPTP monitoring
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
sudo pmc -u -b 0 'GET PARENT_DATA_SET'
sudo pmc -u -b 0 'GET TIME_PROPERTIES_DATA_SET'
```

### GPS/PPS Monitoring
```bash
# PPS signal quality
sudo ppstest /dev/pps0

# GPS NMEA sentences
sudo cat /dev/ttyACM0

# GPS fix status
cgps -s  # If gpsd is running
```

### PHC Clock Comparison
```bash
# Compare PHC to system clock
sudo phc_ctl /dev/ptp0 get
date +%s.%N

# Monitor offset
watch -n 1 'sudo phc2sys -s CLOCK_REALTIME -c /dev/ptp0 -m -O 0'
```

### System Clock Sync Status
```bash
# Chrony tracking
chronyc tracking
chronyc sources
chronyc sourcestats

# NTP status
timedatectl status
```

---

## 📁 File Structure

```
examples/12-RasPi5_i226-grandmaster/
├── README.md                          # This file
├── console.log                        # System status log
├── CMakeLists.txt                     # Build configuration
├── src/
│   ├── ptp_grandmaster.cpp           # Main application
│   ├── linux_ptp_hal.cpp             # Linux HAL implementation
│   ├── linux_ptp_hal.hpp
│   ├── gps_adapter.cpp               # GPS time source adapter
│   ├── gps_adapter.hpp
│   ├── rtc_adapter.cpp               # RTC holdover adapter
│   └── rtc_adapter.hpp
├── boot/
│   └── firmware/
│       └── config.txt                # Boot configuration
├── etc/
│   ├── chrony/
│   │   └── chrony.conf               # Chrony GPS+PPS config
│   ├── default/
│   │   └── gpsd                      # GPSD configuration
│   ├── systemd/system/
│   │   ├── ptp4l-gm.service         # LinuxPTP grandmaster
│   │   ├── ptp4l-repo.service       # Repository implementation
│   │   ├── phc2sys-ptp0.service
│   │   └── chrony-waitsync-hwclock.service
│   └── udev/rules.d/
│       └── 99-pps-chrony.rules      # PPS device permissions
└── info/                             # Documentation
```

---

## 🚀 Quick Start

### 1. Connect Hardware
- Connect network cable to eth1
- Ensure GPS antenna has clear sky view
- Verify GPS module USB connection

### 2. Start Services
```bash
# Enable and start services
sudo systemctl enable ptp4l-gm.service phc2sys-ptp0.service
sudo systemctl start ptp4l-gm.service phc2sys-ptp0.service

# Check status
sudo systemctl status ptp4l-gm.service
```

### 3. Verify Operation
```bash
# Check PTP state
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'

# Monitor logs
sudo journalctl -u ptp4l-gm.service -f
```

---

## 📚 References

- **IEEE 1588-2019**: Precision Time Protocol specification
- **LinuxPTP**: https://linuxptp.sourceforge.net/
- **Intel i226 Datasheet**: Hardware timestamping capabilities
- **Repository Documentation**: [../../README.md](../../README.md)
- **GPS Integration Example**: [../04-gps-nmea-sync/](../04-gps-nmea-sync/)
- **ESP32 Grandmaster Example**: [../11-esp32-ptp-grandmaster/](../11-esp32-ptp-grandmaster/)

---

## 🔧 Troubleshooting

### TAI-UTC Offset Configuration

**Issue**: GPS time (UTC-based) differs from PTP time (TAI-based) by current TAI-UTC offset.

**Solution**: The grandmaster automatically retrieves TAI offset from kernel via `adjtimex()` system call.

**Verify kernel TAI offset** (optional - code works without this utility):
```bash
# Check current setting:
adjtimex --print | grep tai
# Should show: tai: 37  (as of Jan 2026)

# If zero, set it:
sudo adjtimex --tai 37

# Alternative verification (no utility needed):
python3 -c "import ctypes; tx = type('timex', (), {'tai': ctypes.c_long(0)})(); libc = ctypes.CDLL('libc.so.6'); libc.adjtimex(ctypes.byref(tx)); print('TAI offset:', tx.tai)"
```

**Note**: Setting persists across reboots on most systems.

### High Latency Warnings

**Root Cause** (identified via strace analysis - see deb.md):
- GPS serial blocking reads with `VTIME=10` (1 second timeout)  
- 100ms `clock_nanosleep()` in main loop
- PHC sampling happens AFTER GPS/RTC processing (70-170ms delayed)

**Solution** (proper architecture):
- **RT Thread** (CPU2, FIFO 80): PPS wait → PHC sample → ringbuffer push
- **Worker Thread** (CPU0/1/3): GPS read/parse, RTC, PTP messaging, logging
- Eliminates blocking GPS reads from critical timing path
- Target PHC sampling latency: < 10ms from PPS edge

**Implementation Status**: ⏳ In progress (threaded architecture)

**Temporary workarounds** (if not using threaded version):
1. **Run without verbose mode** (minimal console output):
   ```bash
   sudo chrt -f 80 taskset -c 2 ./ptp_grandmaster --interface=eth1
   ```

2. **Redirect output** (fastest - zero console overhead):
   ```bash
   sudo chrt -f 80 taskset -c 2 ./ptp_grandmaster --interface=eth1 --verbose >/tmp/gm.log 2>&1
   tail -f /tmp/gm.log  # Monitor in separate terminal
   ```

3. **GPS/RTC logging is rate-limited** to 1 Hz in verbose mode (already optimized)

### PTP Not Achieving MASTER State
```bash
# Check network interface
sudo ip link show eth1
sudo ethtool eth1

# Verify PTP capability
sudo ethtool -T eth1 | grep -i ptp

# Check for competing masters
sudo tcpdump -i eth1 -nn ether proto 0x88f7
```

### GPS Not Providing Time
```bash
# Check GPS connection
ls -l /dev/ttyACM0
sudo cat /dev/ttyACM0  # Should show NMEA sentences

# Check PPS signal
sudo ppstest /dev/pps0
```

### PHC Drift Analysis

**Normal behavior**: PHC drift varies between runs due to:
- **Crystal warm-up**: First run after power-on shows higher drift (40-80 ppm)
- **Thermal stabilization**: Subsequent runs converge to 6-10 ppm (realistic i226 crystal offset)
- **Measurement window**: 20-second calibration is a snapshot, not long-term average

**Typical drift progression**:
```
Run 1 (cold start):  47.8 ppm → Crystal not yet stabilized
Run 2 (warm):         6.7 ppm → Realistic offset
Run 3 (stable):       6.1 ppm → Consistent measurement
```

**Expected PPS performance** (GPS-grade):
- **Jitter**: 1.0-3.6 µs (excellent)
- **Drift**: -0.14 to -0.16 ppm average (sub-ppm accuracy)
- **RTC offset**: -2.4 ms consistent (correctable via aging offset)

**Action required**: None if drift stabilizes <10 ppm after 2-3 runs.

### PHC Drift Too Large
```bash
# Monitor PHC offset
sudo phc2sys -s CLOCK_REALTIME -c /dev/ptp0 -m

# Check GPS lock quality
chronyc sourcestats
```

---

## 🧪 Testing with Slave Device

### Prerequisites
- **Two Raspberry Pi 5 devices** with Intel i226 NICs
- **Direct Ethernet connection** or same network switch
- **Grandmaster running** on first device
- **ptp4l installed** on second device: `sudo apt install linuxptp`

### Slave Device Setup

**1. Identify connected interface**:
```bash
ip link show
# Look for "UP,LOWER_UP" (cable connected)
# Example: eth1: <BROADCAST,MULTICAST,UP,LOWER_UP>
```

**2. Verify PHC device mapping**:
```bash
readlink -f /sys/class/net/eth1/ptp
# Example output: /sys/class/ptp/ptp1 → Use /dev/ptp1
```

### Run Slave Test

**Basic synchronization** (use YOUR connected interface from step 1):
```bash
sudo ptp4l -i eth1 -s -m

# Flags:
# -i eth1  : Use eth1 interface (change if needed!)
# -s       : Slave mode only
# -m       : Print monitoring statistics
```

**Expected output** (after 30-60 seconds):
```
ptp4l: rms    234 max    456 freq  +12345 +/-  123 delay    456 +/-   12
       ^^^                ^^^^^^
       RMS offset         Frequency correction
```

**Success criteria**:
- ✅ **Excellent**: rms < 100 ns (sub-100 nanosecond sync)
- ✅ **Good**: rms < 1000 ns (sub-microsecond sync - goal achieved)
- ✅ **Frequency stabilizes** after 60 seconds
- ✅ **Path delay** < 10 µs (direct connection)

**Verbose debugging**:
```bash
sudo ptp4l -i eth1 -s -m -l 7
# Shows: Announce, Sync, Follow_Up message reception
```

**Troubleshooting**:
- **No master clock found**: Check grandmaster is running and cable connected
- **High offset jitter**: Verify hardware timestamping enabled
- **Wrong interface**: Use `ip link show` to find connected interface (must show "UP,LOWER_UP")

---

## 📝 Development Notes

**Current Status**: Phase 1 (Baseline Validation)
**Next Steps**: 
1. Connect eth1 network cable
2. Validate LinuxPTP grandmaster operation
3. Build repository code for ARM64
4. Implement Linux HAL adapter
5. Create grandmaster application
6. Test IEEE 1588-2019 compliance

**Target Accuracy**: < 100 ns to GPS time
**Expected Performance**: 
- Sync accuracy: ±50 ns (hardware timestamping)
- Holdover: < 1 µs drift per minute (DS3231 RTC)
- Network jitter: < 1 µs (Ethernet)
