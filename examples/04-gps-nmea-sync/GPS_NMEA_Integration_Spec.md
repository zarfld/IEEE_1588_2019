# GPS Module (Serial NMEA) Synchronization Example

**Purpose**: Demonstrate IEEE 1588-2019 PTP clock synchronization with GPS reference time input via Serial NMEA interface.

**Status**: Planned for Day 2-3 (Nov 13-14, 2025)  
**Target Release**: v1.0.0-MVP  
**Hardware Required**: GPS receiver with NMEA-0183 serial output

---

## 🎯 Objectives

1. **Demonstrate Real-World PTP Use Case** - Synchronize PTP Ordinary Clock with GPS reference
2. **Validate Hardware Abstraction Layer** - Prove HAL design works with actual hardware
3. **Measure Synchronization Accuracy** - Quantify PTP offset vs GPS time
4. **Provide Reference Implementation** - Example for users integrating GPS timing sources

---

## 📋 Requirements

### Functional Requirements

**FR-GPS-001**: Parse NMEA-0183 sentences from serial GPS module  
- Support GPRMC (Recommended Minimum Specific GPS/Transit Data)
- Support GPGGA (Global Positioning System Fix Data)
- Extract UTC time with millisecond precision

**FR-GPS-002**: Convert GPS UTC time to PTP timestamp format  
- IEEE 1588-2019 timestamp (seconds + nanoseconds)
- Handle UTC leap seconds properly
- Account for GPS-UTC offset (currently 18 seconds as of 2024)

**FR-GPS-003**: Update PTP Ordinary Clock with GPS reference time  
- Set PTP clock using GPS time as Grandmaster
- Calculate and apply offset correction
- Maintain IEEE 1588-2019 Best Master Clock Algorithm (BMCA) compliance

**FR-GPS-004**: Serial port communication via HAL abstraction  
- Hardware-agnostic serial interface
- Configurable baud rate (default: 9600 bps for NMEA)
- Non-blocking reads with timeout

### Non-Functional Requirements

**NFR-GPS-001**: **Accuracy** - Synchronization within ±100 microseconds of GPS time  
**NFR-GPS-002**: **Latency** - Process NMEA sentence within 10ms of reception  
**NFR-GPS-003**: **Reliability** - Handle GPS signal loss gracefully (continue with last known offset)  
**NFR-GPS-004**: **Portability** - Compile on Windows, Linux, and embedded RTOS

---

## 🏗️ Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────┐
│                 GPS NMEA Example                     │
├─────────────────────────────────────────────────────┤
│                                                       │
│  ┌──────────────┐         ┌───────────────────┐    │
│  │ GPS Hardware │────────▶│ Serial HAL        │    │
│  │ (NMEA-0183)  │  RS232  │ Abstraction       │    │
│  └──────────────┘         └─────────┬─────────┘    │
│                                      │              │
│                                      ▼              │
│                            ┌──────────────────┐    │
│                            │ NMEA Parser      │    │
│                            │ (GPRMC/GPGGA)    │    │
│                            └─────────┬────────┘    │
│                                      │              │
│                                      ▼              │
│                            ┌──────────────────┐    │
│                            │ GPS Time Extract │    │
│                            │ & UTC Conversion │    │
│                            └─────────┬────────┘    │
│                                      │              │
│                                      ▼              │
│  ┌──────────────────────────────────────────────┐  │
│  │   IEEE 1588-2019 PTP Ordinary Clock          │  │
│  │   - OrdinaryClock::set_time()                │  │
│  │   - Apply offset correction                  │  │
│  │   - Maintain BMCA state                      │  │
│  └──────────────────────────────────────────────┘  │
│                                                     │
└─────────────────────────────────────────────────────┘
```

### Data Flow

1. **GPS Module** → Serial NMEA sentence (1Hz typical)
2. **Serial HAL** → Read NMEA string into buffer
3. **NMEA Parser** → Extract time fields (HHMMSS.sss + Date)
4. **GPS Time Converter** → Convert to PTP timestamp + apply leap seconds
5. **PTP Clock** → Update clock offset, run BMCA

---

## 🔧 Implementation Plan

### Files to Create

```
examples/04-gps-nmea-sync/
├── README.md                          # Setup instructions, hardware requirements
├── gps_nmea_sync_example.cpp          # Main example application
├── nmea_parser.hpp                    # NMEA sentence parsing
├── nmea_parser.cpp
├── gps_time_converter.hpp             # GPS → PTP timestamp conversion
├── gps_time_converter.cpp
├── serial_hal_interface.hpp           # Serial port HAL abstraction
├── serial_hal_windows.cpp             # Windows serial port implementation
├── serial_hal_linux.cpp               # Linux serial port implementation
├── CMakeLists.txt                     # Build configuration
└── tests/
    ├── test_nmea_parser.cpp           # Unit tests for NMEA parsing
    ├── test_gps_time_converter.cpp    # Unit tests for time conversion
    └── test_integration.cpp           # Integration test with simulated GPS
```

### NMEA Parser Interface

```cpp
namespace GPS {
namespace NMEA {

struct GPSFix {
    uint8_t hour;        // 0-23 UTC
    uint8_t minute;      // 0-59
    uint8_t second;      // 0-59
    uint16_t millisecond; // 0-999
    uint16_t year;       // Full year (e.g., 2025)
    uint8_t month;       // 1-12
    uint8_t day;         // 1-31
    double latitude;     // Decimal degrees
    double longitude;    // Decimal degrees
    bool valid;          // GPS fix valid flag
};

class NMEAParser {
public:
    /**
     * Parse GPRMC sentence
     * @param sentence NMEA sentence string (e.g., "$GPRMC,123519.000,A,...")
     * @param fix Output GPS fix data
     * @return 0 on success, negative error code on failure
     */
    int parse_gprmc(const char* sentence, GPSFix& fix);
    
    /**
     * Parse GPGGA sentence
     * @param sentence NMEA sentence string
     * @param fix Output GPS fix data
     * @return 0 on success, negative error code on failure
     */
    int parse_gpgga(const char* sentence, GPSFix& fix);
    
private:
    bool validate_checksum(const char* sentence);
    void extract_time(const char* time_str, GPSFix& fix);
    void extract_date(const char* date_str, GPSFix& fix);
};

} // namespace NMEA
} // namespace GPS
```

### GPS Time Converter Interface

```cpp
namespace GPS {

class TimeConverter {
public:
    /**
     * Convert GPS fix to PTP timestamp
     * @param fix GPS time from NMEA parser
     * @param ptp_timestamp Output PTP timestamp (IEEE 1588-2019 format)
     * @return 0 on success, negative error code on failure
     */
    int gps_to_ptp_timestamp(const NMEA::GPSFix& fix,
                            IEEE::_1588::PTP::_2019::Timestamp& ptp_timestamp);
    
    /**
     * Calculate offset between PTP clock and GPS time
     * @param gps_time GPS reference time
     * @param ptp_time Current PTP clock time
     * @return Offset in nanoseconds (PTP - GPS)
     */
    int64_t calculate_offset_ns(const IEEE::_1588::PTP::_2019::Timestamp& gps_time,
                                 const IEEE::_1588::PTP::_2019::Timestamp& ptp_time);
    
    /**
     * Get current GPS-UTC leap second offset
     * @return Number of leap seconds (18 as of 2024)
     */
    static constexpr int get_leap_seconds() { return 18; }
};

} // namespace GPS
```

### Serial HAL Interface

```cpp
namespace HAL {
namespace Serial {

struct SerialConfig {
    uint32_t baud_rate;      // e.g., 9600, 115200
    uint8_t data_bits;       // 7 or 8
    uint8_t stop_bits;       // 1 or 2
    char parity;             // 'N', 'E', 'O'
    uint32_t timeout_ms;     // Read timeout
};

class SerialInterface {
public:
    virtual ~SerialInterface() = default;
    
    /**
     * Open serial port
     * @param port_name Port name (e.g., "COM3", "/dev/ttyUSB0")
     * @param config Serial port configuration
     * @return 0 on success, negative error code on failure
     */
    virtual int open(const char* port_name, const SerialConfig& config) = 0;
    
    /**
     * Close serial port
     */
    virtual void close() = 0;
    
    /**
     * Read data from serial port (non-blocking with timeout)
     * @param buffer Output buffer
     * @param max_length Maximum bytes to read
     * @param bytes_read Actual bytes read
     * @return 0 on success, negative error code on failure
     */
    virtual int read(char* buffer, size_t max_length, size_t& bytes_read) = 0;
    
    /**
     * Read complete line (wait for '\n' or timeout)
     * @param buffer Output buffer
     * @param max_length Maximum bytes to read
     * @return 0 on success, negative error code on failure
     */
    virtual int read_line(char* buffer, size_t max_length) = 0;
};

} // namespace Serial
} // namespace HAL
```

---

## 🧪 Testing Strategy

### Unit Tests

1. **NMEA Parser Tests**
   - Valid GPRMC sentence parsing
   - Valid GPGGA sentence parsing
   - Checksum validation
   - Malformed sentence handling
   - Edge cases (midnight rollover, leap seconds)

2. **GPS Time Converter Tests**
   - GPS to PTP timestamp conversion
   - Leap second application
   - Offset calculation accuracy
   - Date/time edge cases

3. **Serial HAL Tests**
   - Port open/close
   - Read with timeout
   - Line buffering
   - Error handling

### Integration Tests

1. **Simulated GPS Stream**
   - Generate synthetic NMEA sentences
   - Feed to parser → converter → PTP clock
   - Verify PTP clock synchronization

2. **Real Hardware Test**
   - Connect actual GPS receiver
   - Run for 1 hour minimum
   - Measure synchronization accuracy
   - Log offset drift over time

### Hardware Test Setup

**Required Hardware**:
- GPS receiver with NMEA-0183 output (e.g., u-blox NEO-6M, NEO-M8N)
- USB-to-Serial adapter (if needed)
- PPS (Pulse-Per-Second) output (optional, for sub-microsecond validation)

**Test Procedure**:
1. Connect GPS module to serial port
2. Wait for GPS lock (LED indicator)
3. Run example: `./gps_nmea_sync_example --port COM3 --baud 9600`
4. Monitor PTP offset in real-time
5. Log results to CSV for analysis

**Expected Results**:
- GPS lock within 60 seconds (outdoors, clear sky)
- PTP synchronization within ±100 μs of GPS time
- Stable offset over 1-hour test (< 10 μs drift)

---

## 📊 Success Criteria

- [ ] **Compilation**: Example compiles on Windows and Linux without errors
- [ ] **Unit Tests**: All NMEA parser and converter tests pass
- [ ] **Simulated GPS**: Integration test with synthetic NMEA stream passes
- [ ] **Real Hardware**: Synchronization within ±100 μs of GPS time
- [ ] **Documentation**: Complete README with hardware setup instructions
- [ ] **IEEE 1588-2019 Compliance**: PTP clock maintains BMCA state correctly

---

## 🚀 Usage Example

```cpp
#include "gps_nmea_sync_example.hpp"
#include <iostream>

int main(int argc, char** argv) {
    // Parse command line
    std::string port = (argc > 1) ? argv[1] : "COM3";
    uint32_t baud = (argc > 2) ? std::stoi(argv[2]) : 9600;
    
    // Initialize PTP Ordinary Clock
    IEEE::_1588::PTP::_2019::Clocks::OrdinaryClock ptp_clock;
    ptp_clock.initialize();
    
    // Initialize GPS NMEA synchronizer
    GPS::NMEASynchronizer gps_sync(port, baud);
    
    // Main synchronization loop
    while (true) {
        // Read NMEA sentence from GPS
        GPS::NMEA::GPSFix fix;
        if (gps_sync.read_and_parse(fix) == 0 && fix.valid) {
            // Convert GPS time to PTP timestamp
            IEEE::_1588::PTP::_2019::Timestamp gps_timestamp;
            GPS::TimeConverter::gps_to_ptp_timestamp(fix, gps_timestamp);
            
            // Get current PTP clock time
            auto ptp_timestamp = ptp_clock.get_time();
            
            // Calculate offset
            int64_t offset_ns = GPS::TimeConverter::calculate_offset_ns(
                gps_timestamp, ptp_timestamp);
            
            // Apply correction to PTP clock
            ptp_clock.apply_offset_correction(offset_ns);
            
            // Log synchronization status
            std::cout << "GPS Time: " << fix.hour << ":" << fix.minute 
                     << ":" << fix.second << "." << fix.millisecond
                     << " | PTP Offset: " << offset_ns << " ns" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

---

## 📚 References

- **NMEA-0183 Standard**: National Marine Electronics Association protocol specification
- **IEEE 1588-2019**: Section 7.2 "PTP timescale" and Section 11.2 "Offset from master"
- **GPS-UTC Offset**: Current leap seconds maintained by IERS (International Earth Rotation Service)
- **u-blox GPS Modules**: Popular GPS receivers with NMEA output

---

## 🔗 Related Work

- Issue #6: Additional HAL Implementations (v1.1.0+)
- Issue #7: Performance Benchmarking Tools (v1.1.0)
- Phase 09: Operation & Maintenance (GPS module monitoring)

---

**Implementation Timeline**:
- Day 2 (Nov 13): NMEA parser + GPS converter + tests
- Day 3 (Nov 14): Real hardware testing + documentation
- Day 4 (Nov 15): Integrate results into release notes

**Assigned to**: Day 2-3 QA tasks  
**Priority**: High (demonstrates real-world PTP use case)  
**Complexity**: Medium (well-defined interfaces, standard protocols)
