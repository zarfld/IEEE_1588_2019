# Implementation Plan: Raspberry Pi 5 PTP Grandmaster

**Project**: IEEE 1588-2019 PTP Grandmaster on Raspberry Pi 5  
**Hardware**: Intel i226 NIC, u-blox GPS, DS3231 RTC, PPS GPIO  
**Status**: Planning Phase  
**Updated**: 2026-01-09

---

## 🎯 Objectives

1. **Integrate** this repository's IEEE 1588-2019 code with Raspberry Pi 5 hardware
2. **Implement** Linux-specific HAL for hardware timestamping
3. **Create** GPS-disciplined grandmaster clock application
4. **Enable** remote debugging capabilities
5. **Validate** IEEE 1588-2019 compliance and timing accuracy

---

## 📋 Prerequisites

### Hardware Status (from console.log analysis)
- ✅ Intel i226 NIC detected and functional
- ✅ PTP hardware clocks available (/dev/ptp0, /dev/ptp1)
- ✅ GPS module working (u-blox G70xx on /dev/ttyACM0)
- ✅ PPS signal stable (/dev/pps0, <2µs jitter)
- ✅ DS3231 RTC configured (I2C)
- ✅ LinuxPTP v4.2 installed
- ⚠️ eth1 network interface DOWN (needs cable connection)

### Software Dependencies
```bash
# Install on Raspberry Pi
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    libgtest-dev \
    gdb \
    gdbserver \
    linux-tools-generic \
    gpsd \
    gpsd-clients \
    pps-tools \
    chrony \
    ethtool
```

---

## 🗂️ Project Structure

```
examples/12-RasPi5_i226-grandmaster/
├── README.md                    # ✅ Created - User guide
├── CMakeLists.txt               # ✅ Created - Build configuration
├── IMPLEMENTATION_PLAN.md       # ✅ This file
├── src/
│   ├── ptp_grandmaster.cpp      # ⏳ TODO - Main application
│   ├── linux_ptp_hal.cpp        # ⏳ TODO - Linux HAL adapter
│   ├── linux_ptp_hal.hpp        # ⏳ TODO - HAL interface
│   ├── gps_adapter.cpp          # ⏳ TODO - GPS time source
│   ├── gps_adapter.hpp          # ⏳ TODO - GPS interface
│   ├── rtc_adapter.cpp          # ⏳ TODO - RTC holdover
│   └── rtc_adapter.hpp          # ⏳ TODO - RTC interface
├── tests/
│   ├── test_linux_hal.cpp       # ⏳ TODO - HAL unit tests
│   ├── test_gps_adapter.cpp     # ⏳ TODO - GPS adapter tests
│   └── test_integration.cpp     # ⏳ TODO - Integration tests
└── etc/                          # ✅ Existing configuration files
```

---

## 📝 Implementation Tasks

### Phase 1: Linux HAL Implementation

**Goal**: Create hardware abstraction layer for Linux PTP stack

#### Task 1.1: Socket Operations (`linux_ptp_hal.cpp`)
- [ ] Create PTP event socket (UDP port 319)
- [ ] Create PTP general socket (UDP port 320)
- [ ] Enable SO_TIMESTAMPING socket options
- [ ] Implement multicast join for PTP addresses
- [ ] Handle socket errors and timeouts

**Reference Code**:
```cpp
// Enable hardware timestamping
struct ifreq hwtstamp;
struct hwtstamp_config hwconfig;

hwconfig.flags = 0;
hwconfig.tx_type = HWTSTAMP_TX_ON;
hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;

strncpy(hwtstamp.ifr_name, "eth1", IFNAMSIZ);
hwtstamp.ifr_data = (void *)&hwconfig;

if (ioctl(sock_fd, SIOCSHWTSTAMP, &hwtstamp) < 0) {
    perror("SIOCSHWTSTAMP failed");
}
```

#### Task 1.2: Hardware Timestamp Extraction
- [ ] Implement TX timestamp retrieval (SO_TIMESTAMPING)
- [ ] Implement RX timestamp retrieval (MSG_ERRQUEUE)
- [ ] Convert kernel timestamps to PTP format
- [ ] Handle timestamp errors and fallbacks

#### Task 1.3: PHC Clock Operations
- [ ] Read PHC time (`clock_gettime(CLOCK_PTP)`)
- [ ] Set PHC time (`clock_settime(CLOCK_PTP)`)
- [ ] Adjust PHC frequency (adjtimex/clock_adjtime)
- [ ] Monitor PHC drift

**Reference**: `/usr/include/linux/ptp_clock.h`

### Phase 2: GPS Time Source Integration

**Goal**: Integrate GPS module as primary time reference

#### Task 2.1: GPS NMEA Parser (`gps_adapter.cpp`)
- [ ] Open serial port (/dev/ttyACM0)
- [ ] Configure baud rate (9600)
- [ ] Parse GPRMC sentences (UTC time)
- [ ] Parse GPGGA sentences (fix quality)
- [ ] Validate checksums
- [ ] Convert UTC to TAI (GPS leap seconds)

**Reuse Code**: `examples/04-gps-nmea-sync/gps_time_converter.cpp`

#### Task 2.2: PPS Signal Handling
- [ ] Open PPS device (/dev/pps0)
- [ ] Read PPS assert timestamps
- [ ] Correlate PPS with NMEA time
- [ ] Detect PPS signal loss
- [ ] Calculate PPS jitter statistics

**Reference**:
```cpp
#include <sys/timepps.h>

pps_handle_t pps_handle;
pps_params_t pps_params;
pps_info_t pps_info;

// Open PPS source
int pps_fd = open("/dev/pps0", O_RDWR);
time_pps_create(pps_fd, &pps_handle);

// Fetch PPS event
time_pps_fetch(pps_handle, PPS_TSFMT_TSPEC, &pps_info, NULL);
// Use pps_info.assert_timestamp
```

#### Task 2.3: GPS Time Discipline
- [ ] Implement GPS time validation
- [ ] Detect GPS fix loss/recovery
- [ ] Calculate GPS time offset
- [ ] Update PTP clock quality based on GPS status
- [ ] Implement GPS time smoothing filter

### Phase 3: RTC Holdover Implementation

**Goal**: Maintain time accuracy during GPS outages via RTC frequency discipline

#### Task 3.1: DS3231 RTC Interface (`rtc_adapter.cpp`)
- [ ] Open RTC device (/dev/rtc1) and I2C bus (/dev/i2c-1)
- [ ] Read RTC time via ioctl(RTC_RD_TIME)
- [ ] Access DS3231 aging offset register (I2C addr 0x68, reg 0x10)
- [ ] Read RTC temperature sensor (registers 0x11-0x12)
- [ ] Implement I2C read/write for aging offset adjustment
- [ ] Set RTC time from GPS

**Reference**:
```cpp
#include <linux/rtc.h>
#include <linux/i2c-dev.h>

int rtc_fd = open("/dev/rtc1", O_RDWR);
int i2c_fd = open("/dev/i2c-1", O_RDWR);
ioctl(i2c_fd, I2C_SLAVE, 0x68); // DS3231 I2C address

struct rtc_time rtc_tm;
ioctl(rtc_fd, RTC_RD_TIME, &rtc_tm);

// Read aging offset register (each LSB = 0.1 ppm)
int8_t aging_offset = i2c_smbus_read_byte_data(i2c_fd, 0x10);

// Write aging offset: positive = slower, negative = faster
// Range: ±127 × 0.1 ppm = ±12.7 ppm
i2c_smbus_write_byte_data(i2c_fd, 0x10, new_offset);
```

#### Task 3.2: RTC Frequency Discipline
- [ ] Measure RTC drift against GPS-disciplined system clock (hourly)
- [ ] Calculate frequency error in parts-per-million (ppm)
- [ ] Apply aging offset correction to DS3231 (±12.7 ppm range)
- [ ] Implement exponential moving average for drift estimation
- [ ] Monitor temperature vs drift correlation
- [ ] Create systemd discipline service for continuous adjustment
- [ ] Log drift measurements for analysis

**DS3231 Aging Offset Calibration**:
```cpp
// Measure drift over measurement interval
double measure_drift_ppm(uint64_t gps_time_ns, uint64_t rtc_time_ns, 
                         uint32_t interval_sec) {
    int64_t error_ns = rtc_time_ns - gps_time_ns;
    return (double)error_ns / ((double)interval_sec * 1000000.0);
}

// Calculate aging offset value
int8_t calculate_aging_offset(double drift_ppm) {
    // DS3231: positive offset makes clock slower
    // Clamp to ±12.7 ppm hardware range
    if (drift_ppm > 12.7) return -127;
    if (drift_ppm < -12.7) return 127;
    return (int8_t)(-drift_ppm / 0.1);
}
```

**Performance Targets**:
- Factory DS3231: ±2 ppm drift → ±7.2 sec/hour error
- After discipline: ±0.5 ppm drift → ±1.8 sec/hour error
- Best case: ±0.1 ppm drift → ±360 ms/hour error

#### Task 3.3: Holdover Strategy
- [ ] Detect GPS signal loss
- [ ] Switch to RTC time source
- [ ] Update clock quality (clockClass = 7 holdover)
- [ ] Track holdover duration
- [ ] Restore GPS when available

### Phase 4: PTP Grandmaster Application

**Goal**: Create complete IEEE 1588-2019 grandmaster implementation

#### Task 4.1: Main Application (`ptp_grandmaster.cpp`)
- [ ] Initialize IEEE 1588-2019 OrdinaryClock
- [ ] Configure as Grandmaster (priority1 = 0)
- [ ] Set clock quality based on GPS status
- [ ] Implement event loop
- [ ] Handle signals (SIGINT, SIGTERM)

**Key Components**:
```cpp
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;

// Create grandmaster clock
Clocks::OrdinaryClock gm_clock(
    hal,
    ClockQuality{
        .clockClass = 6,        // GPS-locked
        .clockAccuracy = 0x21,  // < 100ns
        .offsetScaledLogVariance = 0x4E5D
    }
);

// Configure as grandmaster
gm_clock.set_priority1(0);
gm_clock.set_priority2(0);
gm_clock.set_domain_number(0);
```

#### Task 4.2: Message Transmission
- [ ] Send Announce messages (every 1 second)
- [ ] Send Sync messages (configurable rate)
- [ ] Send Follow_Up messages (after Sync)
- [ ] Handle Delay_Req from slaves
- [ ] Send Delay_Resp messages

#### Task 4.3: BMCA Implementation
- [ ] Implement grandmaster BMCA logic
- [ ] Handle foreign masters (reject/accept)
- [ ] Maintain Best Master Clock identity
- [ ] Update clock datasets

### Phase 5: Remote Debugging Setup

**Goal**: Enable development and debugging from remote machine

#### Task 5.1: GDB Remote Setup
- [ ] Configure gdbserver on Raspberry Pi
- [ ] Create VS Code launch configuration
- [ ] Test remote breakpoints
- [ ] Verify symbol debugging

**.vscode/launch.json**:
```json
{
    "name": "RasPi Remote Debug",
    "type": "cppdbg",
    "request": "launch",
    "program": "/usr/local/bin/ptp_grandmaster",
    "miDebuggerServerAddress": "raspi-ip:2345",
    "sourceFileMap": {
        "/home/zarfld": "${workspaceFolder}"
    }
}
```

#### Task 5.2: SSH Development Environment
- [ ] Configure VS Code Remote-SSH
- [ ] Set up remote compilation
- [ ] Enable remote terminal access
- [ ] Configure file synchronization

#### Task 5.3: Performance Profiling
- [ ] Install linux-perf tools
- [ ] Profile PTP message handling
- [ ] Measure timestamp latency
- [ ] Identify bottlenecks

### Phase 6: Testing & Validation

**Goal**: Verify IEEE 1588-2019 compliance and timing accuracy

#### Task 6.1: Unit Tests
- [ ] Test GPS NMEA parsing
- [ ] Test PPS timestamp handling
- [ ] Test RTC operations
- [ ] Test HAL socket operations
- [ ] Test message construction

#### Task 6.2: Integration Tests
- [ ] GPS to PHC synchronization
- [ ] PHC to system clock sync
- [ ] PTP message transmission/reception
- [ ] Grandmaster state transitions
- [ ] Holdover scenario testing

#### Task 6.3: Compliance Validation
- [ ] Verify Announce message format (IEEE 1588-2019 Table 27)
- [ ] Verify Sync message format (Table 34)
- [ ] Verify clock quality reporting
- [ ] Verify BMCA behavior
- [ ] Test with PTP analyzer tool

#### Task 6.4: Accuracy Measurements
- [ ] Measure GPS to PHC offset
- [ ] Measure PTP slave synchronization
- [ ] Measure PPS jitter
- [ ] Measure packet timestamp accuracy
- [ ] Validate < 100ns target accuracy

---

## 📊 Success Criteria

### Functional Requirements
- ✅ PTP grandmaster achieves MASTER state
- ✅ Announce messages transmitted every 1 second
- ✅ Sync messages transmitted at configured rate
- ✅ Responds to Delay_Req from slaves
- ✅ GPS time disciplined PHC within ±100ns
- ✅ RTC holdover maintains ±1µs accuracy for 1 hour
- ✅ Automatic GPS recovery after outage

### Performance Requirements
- ✅ Timestamp accuracy: < 100 ns (hardware timestamping)
- ✅ PPS jitter: < 2 µs
- ✅ Message processing latency: < 10 µs
- ✅ BMCA decision time: < 100 µs
- ✅ Holdover drift: < 1 µs/minute

### Compliance Requirements
- ✅ IEEE 1588-2019 message format compliance
- ✅ Correct clock quality reporting
- ✅ BMCA algorithm correctness
- ✅ Dataset management per specification

---

## 🛠️ Development Workflow

### 1. Build on Raspberry Pi
```bash
# Clone repository
git clone https://github.com/zarfld/IEEE_1588_2019.git
cd IEEE_1588_2019

# Build main library
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
sudo make install

# Build grandmaster example
cd ../examples/12-RasPi5_i226-grandmaster
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
sudo make install
```

### 2. Test Locally
```bash
# Run with verbose logging
sudo /usr/local/bin/ptp_grandmaster --interface=eth1 --verbose

# Monitor logs
sudo journalctl -f
```

### 3. Remote Debug
```bash
# On Raspberry Pi
sudo gdbserver :2345 /usr/local/bin/ptp_grandmaster --interface=eth1

# On development machine (VS Code)
# F5 to start debugging with "RasPi Remote Debug" configuration
```

---

## 📅 Timeline Estimate

| Phase | Task | Effort | Dependencies |
|-------|------|--------|--------------|
| 1 | Linux HAL | 2-3 days | Linux PTP docs |
| 2 | GPS Integration | 2 days | Example 04 code |
| 3 | RTC Holdover | 1 day | RTC interface |
| 4 | Grandmaster App | 2-3 days | Phases 1-3 |
| 5 | Remote Debug | 1 day | Network access |
| 6 | Testing | 2-3 days | All phases |
| **Total** | | **10-13 days** | |

---

## 🔍 Next Steps

1. **Immediate** (Day 1):
   - [ ] Connect network cable to eth1
   - [ ] Verify LinuxPTP baseline operation
   - [ ] Document current system performance

2. **Short Term** (Week 1):
   - [ ] Implement Linux HAL adapter
   - [ ] Create GPS adapter using example 04 code
   - [ ] Build and test on Raspberry Pi

3. **Medium Term** (Week 2):
   - [ ] Complete grandmaster application
   - [ ] Implement RTC holdover
   - [ ] Setup remote debugging

4. **Validation** (Week 3):
   - [ ] Run compliance tests
   - [ ] Measure timing accuracy
   - [ ] Compare with LinuxPTP performance
   - [ ] Document results

---

## 📚 Reference Materials

### Example Code to Reuse
- `examples/04-gps-nmea-sync/` - GPS NMEA parsing, PPS handling
- `examples/11-esp32-ptp-grandmaster/` - Grandmaster architecture
- `examples/03-hal-implementation-template/` - HAL pattern

### Linux Documentation
- `/usr/include/linux/ptp_clock.h` - PHC operations
- `/usr/include/linux/net_tstamp.h` - Hardware timestamping
- `/usr/include/sys/timepps.h` - PPS API
- LinuxPTP source code - ptp4l implementation reference

### IEEE 1588-2019 Sections
- Section 6: PTP device types
- Section 9.2: State machines
- Section 11: Delay mechanisms
- Section 13: Message formats
- Table 27: Announce message
- Table 34: Sync message

---

**Last Updated**: 2026-01-09  
**Status**: Planning Complete → Ready for Implementation  
**Owner**: Development Team
