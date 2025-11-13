# Multi-Source Time Synchronization Examples

Complete implementation of IEEE 1588-2019 time synchronization using multiple external time sources with Best Master Clock Algorithm (BMCA) selection.

## Overview

This example suite demonstrates how to:
1. **Integrate multiple time sources** (GPS, NTP, DCF77, RTC)
2. **Use BMCA** to automatically select best available source
3. **Implement bidirectional synchronization** (sources can be sinks)
4. **Provide fallback time** during primary source outages
5. **Maintain clock quality** based on IEEE 1588-2019 standards

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    IEEE 1588-2019 PTP Clock                      │
│                  (OrdinaryClock / BoundaryClock)                 │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         │ Types::Timestamp + Types::ClockQuality
                         │
           ┌─────────────┴──────────────┐
           │    BMCA Selection Logic    │
           │  (Best Master Clock Algo)  │
           └─────────────┬──────────────┘
                         │
         ┌───────────────┼───────────────┬───────────────┐
         │               │               │               │
    ┌────▼────┐    ┌────▼────┐    ┌────▼────┐    ┌────▼────┐
    │   GPS   │    │   NTP   │    │  DCF77  │    │   RTC   │
    │ Adapter │    │ Adapter │    │ Adapter │    │ Adapter │
    └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘
         │              │              │              │
         │              │              │              ▼
    Serial/PPS     UDP/Network   GPIO/Radio      I2C Bus
    (GT-U7)        (port 123)    (77.5 kHz)    (DS3231)
```

## Examples Provided

### 07-rtc-module - RTC Adapter
**Hardware**: DS3231/DS1307/PCF8523 RTC module on I2C  
**Purpose**: Bidirectional time synchronization

**RTC as TIME SOURCE** (when better sources unavailable):
```cpp
RTCAdapter rtc(0x68, RTCModuleType::DS3231);
rtc.initialize();

Types::Timestamp time = rtc.get_current_time();  // Read from RTC
Types::ClockQuality quality = rtc.get_clock_quality();  // clockClass 248

ptp_clock.tick(time);
```

**RTC as TIME SINK** (synchronized by GPS/NTP/DCF77):
```cpp
// GPS provides better time
Types::Timestamp gps_time = gps.get_current_time();
Types::ClockQuality gps_quality = gps.get_clock_quality();  // clockClass 6

// BMCA: GPS better than RTC (6 < 248)
if (gps_quality.clock_class < rtc.get_clock_quality().clock_class) {
    rtc.set_time(gps_time);  // Write GPS time to RTC
}

// Later: GPS lost, RTC provides fallback
if (!gps.is_synchronized()) {
    Types::Timestamp rtc_time = rtc.get_current_time();  // RTC maintains time
    ptp_clock.tick(rtc_time);
}
```

**Build and Run**:
```bash
cd build
cmake --build . --target rtc_ptp_sync_example
./rtc_ptp_sync_example
```

### 08-multi-source-bmca - Multi-Source BMCA Selection
**Hardware**: All adapters (GPS + NTP + DCF77 + RTC)  
**Purpose**: Automatic best source selection with failover

**BMCA Priority** (IEEE 1588-2019 clockClass):
| Priority | Source | clockClass | Condition |
|----------|--------|------------|-----------|
| 🥇 **Best** | GPS | 6 | 3D fix, 9+ satellites |
| 🥈 **Best** | DCF77 | 6 | Strong signal (>80%) |
| 🥉 | GPS | 7 | Holdover < 10 min |
| 4️⃣ | DCF77 | 13 | Weak signal (50-80%) |
| 5️⃣ | NTP | 52 | Stratum 1 server |
| 6️⃣ | RTC | 52 | Recently synced (<1 hour) |
| 7️⃣ | NTP | 187 | Stratum 2+ servers |
| 8️⃣ | RTC | 187 | Holdover (<24 hours) |
| ⚠️ **Fallback** | RTC | 248 | Standalone (>24 hours) |

**Complete Example**:
```cpp
// Initialize all adapters
GPSAdapter gps("/dev/ttyUSB0");
NTPAdapter ntp("pool.ntp.org");
DCF77Adapter dcf77(GPIO_PIN);
RTCAdapter rtc(0x68, RTCModuleType::DS3231);

// Update all sources
gps.update();
ntp.update();
dcf77.update();
rtc.update();

// Collect available sources
std::vector<TimeSource> sources;
if (gps.is_synchronized()) {
    sources.push_back({gps.get_current_time(), gps.get_clock_quality()});
}
// ... NTP, DCF77, RTC ...

// BMCA: Select best source
auto best = select_best_source(sources);  // Based on clockClass

// Synchronize RTC with best source
if (best.quality.clock_class < 100) {  // Better than RTC alone
    rtc.set_time(best.time);  // RTC as SINK
}

// Use best source for PTP clock
ptp_clock.get_default_data_set().clockQuality = best.quality;
ptp_clock.tick(best.time);
```

**Build and Run**:
```bash
cd build
cmake --build . --target multi_source_sync_example
./multi_source_sync_example --gps /dev/ttyUSB0 --ntp pool.ntp.org --rtc
```

## Hardware Requirements

### Required
- **RTC Module** (DS3231 recommended)
  - I2C address: 0x68
  - Accuracy: ±2ppm TCXO
  - Battery backup: CR2032
  - Cost: ~$5-10

### Optional (at least one recommended)
- **GPS Module** (GT-U7)
  - Serial UART
  - Accuracy: ±100ns with PPS
  - Cost: ~$10-20

- **NTP Network**
  - UDP port 123
  - Accuracy: ±10-100ms
  - Cost: Free (internet)

- **DCF77 Receiver** (Europe only)
  - GPIO for pulse detection
  - Accuracy: ±1ms
  - Range: ~2000km from Germany
  - Cost: ~$20-30

## Wiring

### DS3231 RTC Module (I2C)
```
DS3231 → Microcontroller
VCC    → 3.3V or 5V
GND    → GND
SDA    → I2C SDA pin (with 4.7kΩ pull-up)
SCL    → I2C SCL pin (with 4.7kΩ pull-up)
```

**ESP32**: SDA=GPIO21, SCL=GPIO22  
**Arduino**: SDA=A4, SCL=A5  
**Raspberry Pi**: SDA=GPIO2, SCL=GPIO3

## Clock Quality Comparison

| Source | Accuracy | Holdover | clockClass | Best Use Case |
|--------|----------|----------|------------|---------------|
| **GPS** | ±100ns | 10 min | 6-7 | Outdoor, high precision |
| **DCF77** | ±1ms | 1 hour | 6-13 | Europe, reliable |
| **NTP** | ±10ms | 30 sec | 52-187 | Network available |
| **RTC** | ±2ppm | Days | 52-248 | Fallback, persistence |

### Drift Accumulation Examples

**DS3231 RTC (±2ppm)**:
- 1 hour: ±7.2 µs
- 1 day: ±172.8 µs
- 1 week: ±1.2 ms

**GPS Holdover (TCXO)**:
- 1 min: ±10 µs
- 10 min: ±100 µs (holdover limit)

## Usage Scenarios

### Scenario 1: Outdoor Installation with GPS Primary
```
GPS (primary) → RTC (backup)
- Normal: GPS provides time (clockClass 6)
- Night/Clouds: GPS holdover (clockClass 7)
- GPS lost: RTC fallback (clockClass 52 → 248)
```

### Scenario 2: Indoor Installation with NTP Primary
```
NTP (primary) → RTC (backup)
- Normal: NTP provides time (clockClass 52)
- Network down: RTC fallback (clockClass 52 → 248)
- Power cycle: RTC maintains time across boot
```

### Scenario 3: Europe with DCF77 Primary
```
DCF77 (primary) → RTC (backup)
- Normal: DCF77 provides time (clockClass 6)
- Interference: DCF77 degrades (clockClass 13)
- Night: RTC fallback (clockClass 52 → 248)
```

### Scenario 4: Multi-Source Redundancy
```
GPS + NTP + RTC (BMCA selects best)
- Outdoor: GPS wins (clockClass 6)
- Indoor: NTP wins (clockClass 52)
- Offline: RTC wins (clockClass 248)
```

## API Summary

### Common Interface (All Adapters)

```cpp
class TimeSourceAdapter {
    bool initialize();
    bool update();
    
    // Read operations (SOURCE)
    Types::Timestamp get_current_time() const;
    Types::ClockQuality get_clock_quality() const;
    uint8_t get_time_source() const;
    bool is_synchronized() const;
    
    // Write operation (SINK) - RTC only
    bool set_time(const Types::Timestamp& time);  // RTCAdapter only
};
```

### BMCA Selection

```cpp
bool compare_clock_quality(const TimeSource& a, const TimeSource& b) {
    // IEEE 1588-2019 Section 9.3.4
    if (a.clock_class != b.clock_class)
        return a.clock_class < b.clock_class;  // Lower is better
    
    if (a.clock_accuracy != b.clock_accuracy)
        return a.clock_accuracy < b.clock_accuracy;
    
    return a.offset_scaled_log_variance < b.offset_scaled_log_variance;
}
```

## Integration with PTP

```cpp
// 1. Get time and quality from best source
Types::Timestamp best_time = best_source->get_current_time();
Types::ClockQuality best_quality = best_source->get_clock_quality();

// 2. Update PTP clock DefaultDataSet
auto& default_ds = ptp_clock.get_default_data_set();
default_ds.clockQuality = best_quality;

// 3. Update PTP clock TimePropertiesDataSet
auto& time_props_ds = ptp_clock.get_time_properties_data_set();
time_props_ds.timeSource = best_source->get_time_source();

// 4. Update PTP clock time
ptp_clock.tick(best_time);
```

## Testing

### Test RTC Adapter Alone
```bash
./rtc_ptp_sync_example
# Expected: RTC provides time with clockClass 248
```

### Test GPS → RTC Synchronization
```bash
./multi_source_sync_example --gps /dev/ttyUSB0 --rtc
# Expected: GPS clockClass 6 selected, RTC synchronized
# Disconnect GPS: RTC continues with clockClass 52
```

### Test Multi-Source Failover
```bash
./multi_source_sync_example --gps /dev/ttyUSB0 --ntp pool.ntp.org --rtc
# Expected: GPS selected (clockClass 6)
# Disconnect GPS: NTP selected (clockClass 52)
# Disconnect network: RTC selected (clockClass 52 → 248)
```

## Troubleshooting

### RTC Not Detected
```
ERROR: Failed to initialize RTC module
```
- Check I2C wiring (SDA, SCL, pull-ups)
- Verify I2C address (0x68 for DS3231)
- Test with i2cdetect: `i2cdetect -y 1`

### RTC Time Incorrect
```
RTC Time: 2000-01-01 00:00:00 UTC
```
- RTC battery dead or not installed
- RTC never synchronized
- Run: `rtc_ptp_sync_example` to set time

### All Sources Unavailable
```
ERROR: No time sources available!
```
- At minimum, RTC must be initialized
- Check hardware connections
- Verify CMake built all adapters

## References

- IEEE 1588-2019 Standard (PTP v2.1)
- IEEE 1588-2019 Section 9.3 - Best Master Clock Algorithm
- IEEE 1588-2019 Table 5 - clockClass enumeration
- IEEE 1588-2019 Table 6 - timeSource enumeration
- DS3231 Datasheet - Maxim Integrated
- GT-U7 GPS Module Documentation
- RFC 5905 - Network Time Protocol v4
- DCF77 Time Signal Specification

## Next Steps

1. **Add your own adapter** - Follow the pattern in `rtc_adapter.hpp`
2. **Implement platform I2C** - Replace placeholder I2C functions
3. **Add drift compensation** - Use temperature sensor in DS3231
4. **Implement BMCA extensions** - Add transparent clock support
5. **Test holdover performance** - Measure actual drift vs specifications

## License

This implementation follows IEEE 1588-2019 specification requirements.
See repository root LICENSE file for details.
