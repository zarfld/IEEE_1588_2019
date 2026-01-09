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
  - NMEA output: Working (9600 baud)
- ✅ **LinuxPTP**: v4.2 installed
  - `ptp4l`, `phc2sys`, `ts2phc` available
- ✅ **Chrony**: Configured with GPS+PPS
  - SHM 0: GPS time via NMEA
  - PPS: `/dev/pps0` locked to GPS
  - RTC sync enabled

### 🔧 Configuration Status
- ✅ SystemD services created (not yet enabled):
  - `ptp4l-gm.service` - PTP grandmaster daemon
  - `phc2sys-eth1.service` - PHC to system clock sync
  - `phc2sys-ptp0.service` - System clock to PHC sync
- ✅ Chrony configured for GPS+PPS
- ⚠️ **eth1 DOWN** - Network cable needs connection
- ⚠️ **RTC frequency not disciplined yet** - DS3231 aging offset needs calibration from GPS drift measurement
- ⚠️ **Repository code not integrated yet** - Using standard LinuxPTP only

**RTC Discipline Plan**:
- Measure DS3231 drift against GPS-disciplined system clock (48 hours)
- Calculate frequency error in ppm (parts-per-million)
- Apply aging offset correction via I2C register 0x10 (±12.7 ppm range)
- Continuous discipline service for ongoing adjustment
- Target: <±0.5 ppm drift for accurate holdover

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

### Phase 2: Repository Code Integration
**Objective**: Replace LinuxPTP with IEEE 1588-2019 repository implementation

#### 2.1 Build Repository Code for ARM64

```bash
# On development machine (cross-compile) or directly on Pi
git clone https://github.com/zarfld/IEEE_1588_2019.git
cd IEEE_1588_2019

# Configure for Raspberry Pi
mkdir build-raspi && cd build-raspi
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DBUILD_EXAMPLES=ON \
  -DENABLE_HARDWARE_TIMESTAMPING=ON

# Build
make -j4

# Install
sudo make install
```

#### 2.2 Create Linux HAL Adapter

**File**: `examples/12-RasPi5_i226-grandmaster/src/linux_ptp_hal.cpp`

Implements hardware abstraction for:
- Socket creation and binding
- Hardware timestamping (SO_TIMESTAMPING)
- PTP message send/receive
- PHC clock operations (clock_gettime/clock_settime)
- GPIO PPS interrupt handling

**Reference**: Similar to `examples/04-gps-nmea-sync/gps_ptp_sync_example.cpp`

#### 2.3 Create Grandmaster Application

**File**: `examples/12-RasPi5_i226-grandmaster/src/ptp_grandmaster.cpp`

```cpp
/*
 * IEEE 1588-2019 PTP Grandmaster for Raspberry Pi 5
 * 
 * Hardware:
 *   - Intel i226 NIC (eth1, /dev/ptp0)
 *   - u-blox GPS (/dev/ttyACM0, /dev/pps0)
 *   - DS3231 RTC (I2C holdover)
 * 
 * Implements:
 *   - IEEE 1588-2019 Grandmaster Clock
 *   - GPS time synchronization
 *   - RTC holdover
 *   - Hardware timestamping
 *   - BMCA with GPS priority
 */

#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"
#include "linux_ptp_hal.hpp"
#include "gps_adapter.hpp"
#include "rtc_adapter.hpp"

int main(int argc, char** argv) {
    // Initialize GPS time source
    GpsAdapter gps("/dev/ttyACM0", "/dev/pps0");
    
    // Initialize RTC holdover
    RtcAdapter rtc("/dev/rtc1"); // DS3231
    
    // Initialize Linux PTP HAL
    LinuxPtpHal hal("eth1", "/dev/ptp0");
    
    // Create IEEE 1588-2019 Grandmaster Clock
    IEEE::_1588::PTP::_2019::Clocks::OrdinaryClock gm_clock(
        hal,
        IEEE::_1588::PTP::_2019::ClockQuality{
            .clockClass = 6,        // GPS-locked primary reference
            .clockAccuracy = 0x21,  // < 100ns accuracy
            .offsetScaledLogVariance = 0x4E5D // GPS jitter variance
        }
    );
    
    // Main event loop
    while (running) {
        // Update from GPS
        if (gps.has_fix()) {
            auto gps_time = gps.get_ptp_time();
            hal.set_phc_time(gps_time);
            gm_clock.update_time_source(TimeSource::GPS);
        } else {
            // GPS lost - use RTC holdover
            gm_clock.update_time_source(TimeSource::INTERNAL_OSCILLATOR);
        }
        
        // Process PTP messages
        gm_clock.process_events();
        
        // Send periodic messages
        gm_clock.send_announce();
        gm_clock.send_sync();
    }
    
    return 0;
}
```

#### 2.4 Update SystemD Service

**File**: `etc/systemd/system/ptp4l-repo.service`

```ini
[Unit]
Description=IEEE 1588-2019 PTP Grandmaster (Repository Implementation)
After=chrony.service gpsd.service
Wants=chrony.service gpsd.service

[Service]
Type=simple
ExecStart=/usr/local/bin/ptp_grandmaster --interface=eth1 --gps=/dev/ttyACM0
Restart=always
RestartSec=5
Nice=-10
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80

[Install]
WantedBy=multi-user.target
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

### PHC Drift Too Large
```bash
# Monitor PHC offset
sudo phc2sys -s CLOCK_REALTIME -c /dev/ptp0 -m

# Check GPS lock quality
chronyc sourcestats
```

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
