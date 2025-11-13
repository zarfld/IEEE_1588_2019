# IEEE 1588-2019 Clock Quality Management for GPS Time Sources

## Overview

This document explains **dynamic clock quality attribute management** for GPS-based PTP Grandmaster clocks, following **IEEE 1588-2019** and **IEEE 802.1AS-2021** specifications.

### Critical Principle

**Clock quality attributes MUST accurately reflect the actual performance of your time source.** The IEEE standards do not explicitly state "if GPS lock is lost, set clockClass to X", but they require that the advertised quality attributes truthfully represent the clock's current capabilities.

## IEEE 1588-2019 Clock Quality Attributes

Per **IEEE 1588-2019 Section 8.6.2**, the following attributes describe clock quality for BMCA (Best Master Clock Algorithm):

### 1. clockClass (UInteger8)
**Purpose**: Indicates traceability and quality of the clock source  
**Standard**: IEEE 1588-2019 Section 8.6.2.2, Table 5

| Value | Meaning | When to Use |
|-------|---------|-------------|
| 6 | Primary reference (traceable to primary time source) | GPS locked (3D fix, 4+ satellites) |
| 7 | Primary reference (holdover) | GPS lost but clock stable (<1s ago) |
| 13 | Application-specific traceable source | Secondary time source |
| 248 | Default (application-specific, not traceable) | No GPS fix, internal oscillator |

**Our Implementation**:
- **GPS Fix** (AUTONOMOUS_FIX, DGPS_FIX): `clockClass = 6`
- **TIME_ONLY** (no position): `clockClass = 248` (conservative - time-only not fully traceable)
- **NO_FIX**: `clockClass = 248`

### 2. clockAccuracy (Enumeration8)
**Purpose**: Specifies time accuracy relative to TAI/UTC  
**Standard**: IEEE 1588-2019 Section 8.6.2.3, Table 6

| Value | Accuracy | When to Use |
|-------|----------|-------------|
| 0x20 | 25 nanoseconds | DGPS + PPS locked (best case) |
| 0x21 | 100 nanoseconds | GPS + PPS locked (typical hardware timestamping) |
| 0x22 | 250 nanoseconds | GPS + PPS with jitter |
| 0x31 | 10 milliseconds | GPS NMEA-only (no PPS) |
| 0xFE | Unknown | No GPS fix |

**Our Implementation Decision Table**:

| GPS Fix Status | PPS State | clockAccuracy | Actual Performance |
|----------------|-----------|---------------|-------------------|
| NO_FIX | Any | 0xFE (Unknown) | No valid time |
| TIME_ONLY | Failed | 0x31 (10ms) | NMEA centisecond resolution |
| TIME_ONLY | Locked | 0x21 (100ns) | PPS hardware timestamping |
| AUTONOMOUS_FIX | Failed | 0x31 (10ms) | NMEA centisecond resolution |
| AUTONOMOUS_FIX | Locked | 0x21 (100ns) | PPS hardware timestamping |
| DGPS_FIX | Failed | 0x22 (250ns) | DGPS better than GPS alone |
| DGPS_FIX | Locked | 0x20 (25ns) | DGPS + PPS (optimal) |

**Key Insight**: `clockAccuracy` depends **primarily on PPS availability**, not GPS fix quality. A TIME_ONLY fix with PPS is more accurate (100ns) than a DGPS fix without PPS (250ns).

### 3. timeSource (Enumeration8)
**Purpose**: Indicates the source of time  
**Standard**: IEEE 1588-2019 Section 8.6.2.7, Table 8-2

| Value | Source | When to Use |
|-------|--------|-------------|
| 0x10 | ATOMIC_CLOCK | Rubidium, Cesium clock |
| 0x20 | GPS | GPS receiver locked |
| 0x40 | TERRESTRIAL_RADIO | WWVB, DCF77, MSF |
| 0x50 | NTP | Network Time Protocol |
| 0xA0 | INTERNAL_OSCILLATOR | No external time source |

**Our Implementation**:
- **GPS Fix** (TIME_ONLY, AUTONOMOUS_FIX, DGPS_FIX): `timeSource = 0x20` (GPS)
- **NO_FIX**: `timeSource = 0xA0` (INTERNAL_OSCILLATOR)

### 4. offsetScaledLogVariance (UInteger16)
**Purpose**: Logarithmic measure of clock stability (related to Allan Deviation)  
**Standard**: IEEE 1588-2019 Section 8.6.2.4

| Value | Meaning | When to Use |
|-------|---------|-------------|
| 0x4000 | Excellent stability | DGPS + PPS locked |
| 0x4E5D | Good stability | GPS + PPS locked |
| 0x6000 | Moderate stability | DGPS without PPS |
| 0x8000 | Lower stability | GPS without PPS |
| 0xFFFF | Maximum variance (worst) | No GPS fix |

**Formula**: `offsetScaledLogVariance = log₂(variance) × 256`

**Our Implementation**:
- **DGPS + PPS Locked**: `0x4000` (excellent)
- **GPS + PPS Locked**: `0x4E5D` (good)
- **DGPS without PPS**: `0x6000` (moderate)
- **GPS without PPS**: `0x8000` (lower)
- **NO_FIX**: `0xFFFF` (worst)

### 5. priority1 and priority2 (UInteger8)
**Purpose**: BMCA selection priorities (lower value = higher priority)  
**Standard**: IEEE 1588-2019 Sections 8.6.2.1 and 8.6.2.5

| Attribute | Default | Typical Range | Usage |
|-----------|---------|---------------|-------|
| priority1 | 128 | 0-255 | First BMCA comparison (dataset0) |
| priority2 | 128 | 0-255 | Second BMCA comparison (dataset2) |

**Our Implementation**:
- **GPS Locked + PPS**: `priority1 = 100` (high priority)
- **Other states**: `priority1 = 128` (default)
- **All states**: `priority2 = 128` (typically static)

---

## Implementation Example

### Usage in NMEA GPS Application

```cpp
#include "gps_time_converter.hpp"
#include "nmea_parser.hpp"
#include "pps_detector.hpp"

// Initialize components
GPS::Time::GPSTimeConverter converter;
GPS::NMEA::NMEAParser parser;
GPS::PPS::PPSDetector pps_detector(serial_handle);

// Start PPS detection
pps_detector.start_detection(10000);  // 10s timeout

// Main loop
while (running) {
    // Parse NMEA sentence
    GPS::NMEA::GPSTimeData gps_data;
    if (parser.parse_gprmc(nmea_sentence, gps_data)) {
        
        // Update clock quality based on GPS fix and PPS state
        auto pps_state = pps_detector.get_state();
        auto clock_quality = converter.update_clock_quality(
            gps_data.fix_status,
            static_cast<uint8_t>(pps_state)
        );
        
        // Log quality changes
        std::cout << "Clock Quality Updated:\n";
        std::cout << "  clockClass:     " << (int)clock_quality.clock_class << "\n";
        std::cout << "  clockAccuracy:  0x" << std::hex << (int)clock_quality.clock_accuracy << std::dec << "\n";
        std::cout << "  timeSource:     0x" << std::hex << (int)clock_quality.time_source << std::dec << "\n";
        std::cout << "  variance:       0x" << std::hex << clock_quality.offset_scaled_log_variance << std::dec << "\n";
        
        // Update PTP Grandmaster attributes
        update_ptp_grandmaster_quality(clock_quality);
        
        // Convert GPS time to PTP timestamp
        GPS::Time::PTPTimestamp ptp_time;
        if (converter.convert_to_ptp(gps_data, ptp_time)) {
            // Use ptp_time for clock synchronization
            synchronize_ptp_clock(ptp_time);
        }
    }
    
    // Check for GPS lock loss
    if (gps_data.fix_status == GPS::NMEA::GPSFixStatus::NO_FIX) {
        std::cerr << "WARNING: GPS fix lost! Clock quality degraded.\n";
        std::cerr << "  Running on internal oscillator (timeSource=0xA0)\n";
        std::cerr << "  Clock may drift without GPS correction.\n";
    }
}
```

### Integration with PTP Clock

```cpp
void update_ptp_grandmaster_quality(
    const GPS::Time::GPSTimeConverter::ClockQualityAttributes& quality) {
    
    // Update IEEE 1588-2019 defaultDS (Default Data Set)
    auto& defaults = ptp_clock.get_default_data_set();
    
    defaults.clockQuality.clockClass = quality.clock_class;
    defaults.clockQuality.clockAccuracy = quality.clock_accuracy;
    defaults.clockQuality.offsetScaledLogVariance = quality.offset_scaled_log_variance;
    defaults.priority1 = quality.priority1;
    defaults.priority2 = quality.priority2;
    
    // Update timePropertiesDS (Time Properties Data Set)
    auto& time_props = ptp_clock.get_time_properties_data_set();
    time_props.timeSource = quality.time_source;
    
    // Force BMCA re-evaluation
    // Other PTP clocks will see updated quality in next Announce message
    ptp_clock.trigger_announce_update();
}
```

---

## State Transition Scenarios

### Scenario 1: GPS Startup (Cold Start)
```
Time    GPS Fix         PPS State    clockClass  clockAccuracy  timeSource
------  -------------   ----------   ----------  -------------  ----------
T+0s    NO_FIX          Idle         248         0xFE           0xA0
T+30s   TIME_ONLY       Detecting    248         0x31           0x20
T+45s   AUTONOMOUS_FIX  Detecting    6           0x31           0x20
T+48s   AUTONOMOUS_FIX  Locked       6           0x21           0x20  ✓ OPTIMAL
```

### Scenario 2: GPS Signal Loss (Indoor, Tunnel)
```
Time    GPS Fix         PPS State    clockClass  clockAccuracy  timeSource
------  -------------   ----------   ----------  -------------  ----------
T+0s    AUTONOMOUS_FIX  Locked       6           0x21           0x20  ✓ OPTIMAL
T+10s   TIME_ONLY       Locked       248         0x21           0x20  (position lost)
T+30s   NO_FIX          Failed       248         0xFE           0xA0  (signal lost)
        ⚠️ WARNING: Running on internal oscillator, clock will drift!
```

### Scenario 3: PPS Wiring Failure
```
Time    GPS Fix         PPS State    clockClass  clockAccuracy  timeSource
------  -------------   ----------   ----------  -------------  ----------
T+0s    AUTONOMOUS_FIX  Locked       6           0x21           0x20  ✓ OPTIMAL
T+5s    AUTONOMOUS_FIX  Failed       6           0x31           0x20  (PPS lost)
        ⚠️ Accuracy degraded: 100ns → 10ms (still GPS-locked, NMEA-only mode)
```

### Scenario 4: DGPS Upgrade
```
Time    GPS Fix         PPS State    clockClass  clockAccuracy  timeSource
------  -------------   ----------   ----------  -------------  ----------
T+0s    AUTONOMOUS_FIX  Locked       6           0x21           0x20  (100ns)
T+60s   DGPS_FIX        Locked       6           0x20           0x20  (25ns) ✓ BEST
        ✓ Accuracy improved: 100ns → 25ns (DGPS differential corrections)
```

---

## BMCA Impact

### How Clock Quality Affects Master Selection

The **Best Master Clock Algorithm (IEEE 1588-2019 Section 9.3)** selects Grandmaster in this order:

1. **Dataset0**: Compare `priority1` (lower = better)
2. **Dataset1**: Compare `clockClass` → `clockAccuracy` → `offsetScaledLogVariance` (all lower = better)
3. **Dataset2**: Compare `priority2` (lower = better)
4. **Dataset3**: Compare `clockIdentity` (lower = better)

### Example Network Scenario

Consider 3 PTP clocks on a network:

| Clock | GPS Fix | PPS State | clockClass | clockAccuracy | priority1 | Selection |
|-------|---------|-----------|------------|---------------|-----------|-----------|
| A | DGPS_FIX | Locked | 6 | 0x20 (25ns) | 100 | ✓ **WINS** (best quality) |
| B | AUTONOMOUS_FIX | Locked | 6 | 0x21 (100ns) | 100 | 2nd place |
| C | AUTONOMOUS_FIX | Failed | 6 | 0x31 (10ms) | 128 | 3rd place |

**Result**: Clock A becomes Grandmaster (DGPS + PPS = 25ns accuracy)

If Clock A loses GPS lock:

| Clock | GPS Fix | PPS State | clockClass | clockAccuracy | priority1 | Selection |
|-------|---------|-----------|------------|---------------|-----------|-----------|
| A | NO_FIX | Failed | 248 | 0xFE | 128 | 3rd place (no GPS) |
| B | AUTONOMOUS_FIX | Locked | 6 | 0x21 (100ns) | 100 | ✓ **WINS** (now best) |
| C | AUTONOMOUS_FIX | Failed | 6 | 0x31 (10ms) | 128 | 2nd place |

**Result**: Automatic failover to Clock B (GPS + PPS = 100ns accuracy)

---

## Accuracy Comparison: NMEA vs PPS

### NMEA-Only Timing (No PPS)

```
GPS Module                        PTP Clock
┌────────────────┐               ┌──────────────────┐
│ GPRMC sentence │               │ Receive timestamp│
│ 123456.00 UTC  │  (Serial)     │ T1 = ???         │
│ (centiseconds) │───────────────▶│ T2 = ???         │
└────────────────┘               │ T3 = ???         │
                                 └──────────────────┘

Uncertainties:
- NMEA time resolution: ±10ms (centiseconds)
- Serial transmission delay: 1-10ms (variable)
- OS scheduling jitter: 1-100ms (Windows/Linux)
Total Accuracy: ±10-20 milliseconds
```

### PPS + NMEA Timing (Hardware Timestamping)

```
GPS Module                        PTP Clock
┌────────────────┐               ┌──────────────────┐
│ Pin 3: PPS     │  (Hardware)   │ DCD interrupt    │
│ Rising edge ───┼───────────────▶│ Timestamp: T_edge│
│ @ UTC second   │               │ (±10-50ns)       │
└────────────────┘               └──────────────────┘
        │
        ├─ GPRMC sentence (Serial): Which second?
        └─ 123456.00 UTC (absolute time + date)

Combined Result:
- PPS provides precise second boundary: ±10-50 nanoseconds
- NMEA provides absolute time (which second): ±10ms (doesn't matter)
Total Accuracy: ±1 microsecond (PPS dominates)
```

---

## Standards Compliance

### IEEE 1588-2019 Requirements

✅ **Section 8.6.2**: Clock quality attributes implemented  
✅ **Table 5**: clockClass values used correctly  
✅ **Table 6**: clockAccuracy values used correctly  
✅ **Table 8-2**: timeSource enumeration used correctly  
✅ **Section 9.3**: BMCA will use updated quality attributes

### IEEE 802.1AS-2021 Requirements

✅ **Section 8.6.2**: PTP Instance characterization implemented  
✅ **Section 14**: Management data sets updated dynamically  
✅ **Annex B**: Performance requirements considered

---

## Monitoring and Diagnostics

### Log Output Example

```
=== GPS Clock Quality Update ===
Timestamp: 2025-01-15 14:32:45 UTC
GPS Fix Status: AUTONOMOUS_FIX (3D fix, 8 satellites)
PPS Detection State: Locked (1.0002 Hz, 3 edges)

Clock Quality Attributes:
  clockClass:                6 (Primary reference - GPS traceable)
  clockAccuracy:             0x21 (100 nanoseconds)
  offsetScaledLogVariance:   0x4E5D (Good stability)
  timeSource:                0x20 (GPS)
  priority1:                 100 (High priority)
  priority2:                 128 (Default)

BMCA Impact: This clock will be preferred as Grandmaster over:
  - Clocks with clockClass > 6
  - Clocks with clockClass = 6 but clockAccuracy > 0x21
  - Clocks with clockClass = 6, clockAccuracy = 0x21, but priority1 > 100

Timing Performance:
  PTP Accuracy:    ±1 microsecond (PPS hardware timestamping)
  GPS Uncertainty: ±100 nanoseconds (8 satellites, clear sky)
  Holdover:        <10ms/hour (if GPS lost)
```

---

## Best Practices

### 1. **Update Clock Quality on Every State Change**
```cpp
// Whenever GPS fix changes
if (previous_fix_status != current_fix_status) {
    converter.update_clock_quality(current_fix_status, pps_state);
}

// Whenever PPS detection state changes
if (previous_pps_state != current_pps_state) {
    converter.update_clock_quality(gps_fix_status, current_pps_state);
}
```

### 2. **Periodic Quality Updates (Every 1-10 Seconds)**
Even if GPS fix looks stable, clock stability can change:
```cpp
// Update every 5 seconds to reflect gradual degradation
if (time_since_last_update > 5.0) {
    converter.update_clock_quality(gps_fix_status, pps_state);
    time_since_last_update = 0.0;
}
```

### 3. **Holdover Mode (GPS Lost but Clock Stable)**
For high-quality oscillators, maintain quality briefly after GPS loss:
```cpp
if (gps_fix_status == NO_FIX && time_since_gps_loss < 60.0) {
    // Holdover mode: GPS lost but clock still stable
    quality.clock_class = 7;   // Primary reference (holdover)
    quality.clock_accuracy = 0x25;  // 1 microsecond (degrading)
    quality.time_source = 0xA0;  // Internal oscillator
} else if (gps_fix_status == NO_FIX) {
    // GPS lost for >1 minute, fully degraded
    quality.clock_class = 248;  // Default (not traceable)
    quality.clock_accuracy = 0xFE;  // Unknown
}
```

### 4. **Monitor Satellite Count and HDOP**
Better GPS quality → better clock accuracy:
```cpp
if (gps_data.satellites >= 8 && gps_data.hdop < 2.0) {
    // Excellent GPS quality, tighten accuracy
    quality.clock_accuracy = 0x21;  // 100ns
} else if (gps_data.satellites >= 5) {
    // Good GPS quality
    quality.clock_accuracy = 0x22;  // 250ns
} else {
    // Marginal GPS quality
    quality.clock_accuracy = 0x31;  // 10ms
}
```

---

## References

- **IEEE 1588-2019**: *IEEE Standard for a Precision Clock Synchronization Protocol for Networked Measurement and Control Systems* (PTPv2.1)
  - Section 7.2: Timescales (TAI, UTC, GPS)
  - Section 8.6.2: Clock quality attributes
  - Section 9.3: Best Master Clock Algorithm (BMCA)
  - Table 5: clockClass enumeration
  - Table 6: clockAccuracy enumeration
  - Table 8-2: timeSource enumeration

- **IEEE 802.1AS-2021**: *IEEE Standard for Local and Metropolitan Area Networks - Timing and Synchronization for Time-Sensitive Applications* (gPTP)
  - Section 8.6.2: PTP Instance characterization
  - Section 14: Management data sets
  - Annex B: Performance requirements

- **GPS Technical Specifications**:
  - GPS Standard Positioning Service (SPS) Performance Standard, 5th Edition (2020)
  - Timing accuracy: ±40 nanoseconds (95% confidence, clear sky, 6+ satellites)
  - NMEA 0183 Specification (time resolution: centiseconds = 10ms)

---

## Summary

**Key Takeaways**:

1. ✅ **YES, you MUST update clock quality attributes** based on GPS fix status and PPS detection state
2. ✅ **clockAccuracy depends primarily on PPS availability**, not GPS fix quality
3. ✅ **timeSource must indicate actual source** (GPS when locked, INTERNAL_OSCILLATOR when not)
4. ✅ **clockClass indicates traceability** (6 = GPS-traceable, 248 = not traceable)
5. ✅ **BMCA uses these attributes** to select the best Grandmaster
6. ✅ **Standards require accurate quality advertisement** - don't lie about your clock's performance!

**Accuracy Hierarchy** (Best to Worst):
1. **DGPS + PPS Locked**: 25ns (`clockClass=6`, `clockAccuracy=0x20`)
2. **GPS + PPS Locked**: 100ns (`clockClass=6`, `clockAccuracy=0x21`) ← **Your typical case**
3. **GPS without PPS**: 10ms (`clockClass=6`, `clockAccuracy=0x31`)
4. **No GPS (holdover)**: 1ms degrading (`clockClass=7`, `clockAccuracy=0x25`)
5. **No GPS (lost)**: Unknown (`clockClass=248`, `clockAccuracy=0xFE`)

Implement this in your NMEA example, and your PTP Grandmaster will correctly advertise its capabilities to the network! 🚀
