# PPS Detection Testing Strategy and Timing Requirements

## Executive Summary

The PPS detector unit tests complete in **~0.03 seconds** because they are **API-only tests** using mock serial handles. **Real PPS detection requires 2-5+ seconds** due to the need to validate multiple 1Hz pulses.

## Why Current Tests Are Fast (0.03s)

### What Current Tests Do
- **API validation**: Test function signatures, return values, error codes
- **Structure tests**: Validate data structure initialization
- **Mock handles**: Use `nullptr` or fake pointers - no real serial port access
- **Immediate failure**: Detection fails instantly when serial port unavailable
- **No waiting**: Don't wait for actual PPS pulses from hardware

### What Current Tests Don't Do
- ❌ Wait for real PPS pulses (1Hz = 1 second between pulses)
- ❌ Validate edge timestamping accuracy
- ❌ Test frequency validation with actual signals
- ❌ Measure sub-microsecond precision
- ❌ Test platform-specific serial port operations

## Real PPS Detection Timing Requirements

### Design Requirements (from pps_detector.hpp)

```cpp
static constexpr uint32_t MIN_EDGES_FOR_LOCK = 3;  // Require 3 edges (2 intervals)
static constexpr double MIN_INTERVAL_SEC = 0.8;    // ±200ms tolerance
static constexpr double MAX_INTERVAL_SEC = 1.2;    // ±200ms tolerance
```

### Timing Analysis

#### Minimum Detection Time: ~2 seconds

```
Edge 1 (T₀):      First pulse detected
                  ↓ ~1 second (0.8-1.2s with tolerance)
Edge 2 (T₁):      Second pulse detected
                  Interval validation: (T₁ - T₀) ∈ [0.8s, 1.2s]
                  ↓ ~1 second (0.8-1.2s with tolerance)
Edge 3 (T₂):      Third pulse detected
                  Interval validation: (T₂ - T₁) ∈ [0.8s, 1.2s]
                  ↓ LOCK confirmed

Total: ~2 seconds minimum
```

#### Typical Detection Time: 2-5 seconds

**Factors affecting detection time:**

1. **GPS startup time** (0-2s):
   - Cold start: GPS may take 1-2 seconds to output stable PPS
   - Hot start: PPS available immediately
   
2. **Edge alignment** (0-1s):
   - Detection starts mid-second: May miss first edge
   - Must wait for next edge
   
3. **Jitter tolerance** (±200ms per edge):
   - GPS jitter: ±10-50ms typical
   - Platform scheduling: ±50-150ms (Windows/Linux)
   - First interval might be just outside tolerance
   
4. **Detection start timing**:
   - Start at T=0.0s: Catch edge at T=0.0s (best case)
   - Start at T=0.9s: Miss edge, wait until T=1.0s (worst case)

**Realistic timeline:**

```
Scenario 1: Optimal (2.0s)
---------------------------
T=0.000s: Detection starts, Edge 1 detected immediately
T=1.000s: Edge 2 detected, interval valid (1.000s)
T=2.000s: Edge 3 detected, interval valid (1.000s) → LOCKED

Scenario 2: Typical (3.5s)
---------------------------
T=0.000s: Detection starts
T=0.500s: Edge 1 detected (missed start of second)
T=1.500s: Edge 2 detected, interval valid (1.000s)
T=2.500s: Edge 3 detected, interval valid (1.000s) → LOCKED

Scenario 3: GPS Startup (4.5s)
-------------------------------
T=0.000s: Detection starts (GPS not ready)
T=1.500s: GPS stabilizes, Edge 1 detected
T=2.500s: Edge 2 detected, interval valid (1.000s)
T=3.500s: Edge 3 detected, interval valid (1.000s) → LOCKED

Scenario 4: Jitter/Retry (5.5s)
--------------------------------
T=0.000s: Detection starts
T=0.800s: Edge 1 detected
T=1.600s: Edge 2 detected, interval invalid (0.800s - too short)
T=2.600s: Reset, Edge 1
T=3.600s: Edge 2 detected, interval valid (1.000s)
T=4.600s: Edge 3 detected, interval valid (1.000s) → LOCKED
```

#### Maximum Detection Time: 10 seconds (default timeout)

```cpp
// Default timeout in design
detector.start_detection(10000);  // 10 seconds
```

If no valid PPS detected within 10 seconds:
- State transitions to `DetectionState::Failed`
- System falls back to NMEA-only mode (10ms accuracy)
- No sub-microsecond timing available

## Test Categories

### 1. API Unit Tests (Current - 0.03s) ✅ IMPLEMENTED

**File**: `test_pps_detector.cpp`  
**Duration**: ~0.03 seconds  
**Hardware**: None required (uses mock handles)

**Tests**:
- ✅ Construction and initialization
- ✅ Enum conversions
- ✅ Timestamp arithmetic
- ✅ Statistics structures
- ✅ Error handling with invalid handles
- ✅ Thread safety primitives

**Limitations**:
- Does NOT test real PPS signal detection
- Does NOT validate timing accuracy
- Does NOT test hardware serial port operations

### 2. Hardware Validation Tests (TODO - 3-10s) ⏳ NOT IMPLEMENTED

**File**: `test_pps_hardware.cpp` (to be created)  
**Duration**: 3-10 seconds minimum  
**Hardware**: u-blox NEO-G7 GPS on COM3 with PPS connected to DCD pin

**Tests needed**:
- ⏳ Real PPS detection on DCD pin (2-5s)
- ⏳ Edge timestamping accuracy (needs oscilloscope comparison)
- ⏳ 1Hz frequency validation (0.8-1.2s intervals)
- ⏳ Lock confirmation with 3+ edges
- ⏳ Timeout behavior (10s max)
- ⏳ Signal loss detection (2s timeout)
- ⏳ Statistics accuracy (jitter, intervals)

**Example test structure**:
```cpp
bool test_real_pps_detection() {
    // Open real COM3 port
    HANDLE handle = CreateFile("COM3", ...);
    
    PPSDetector detector(handle);
    
    // Start detection with realistic timeout
    std::cout << "Starting PPS detection (expect 2-5 seconds)..." << std::endl;
    bool started = detector.start_detection(10000);
    ASSERT(started);
    
    // Wait for detection to complete (minimum 2 seconds)
    std::this_thread::sleep_for(std::chrono::seconds(6));
    
    // Check if PPS was detected
    if (detector.is_pps_available()) {
        std::cout << "PPS detected on " << detector.get_detected_line() << std::endl;
        
        // Validate timestamp accuracy
        PPSTimestamp ts;
        bool got_timestamp = detector.get_pps_timestamp(2000, ts);
        ASSERT(got_timestamp);
        
        // Check statistics
        auto stats = detector.get_statistics();
        ASSERT(stats.total_edges >= 3);
        ASSERT(stats.valid_intervals >= 2);
        ASSERT(stats.avg_interval_sec >= 0.95 && stats.avg_interval_sec <= 1.05);
        
        return true;
    } else {
        std::cerr << "PPS not detected after 6 seconds!" << std::endl;
        return false;
    }
}
```

### 3. Integration Tests (TODO - 5-15s) ⏳ NOT IMPLEMENTED

**File**: Integration with GPS time converter  
**Duration**: 5-15 seconds (PPS detection + NMEA parsing + PTP conversion)  
**Hardware**: Full GPS module with both NMEA and PPS

**Tests needed**:
- ⏳ PPS + NMEA combined timing
- ⏳ Nanosecond-accurate PTP timestamps
- ⏳ Fallback to NMEA-only when PPS unavailable
- ⏳ Accuracy comparison: NMEA-only (10ms) vs PPS+NMEA (sub-μs)
- ⏳ Continuous operation over minutes/hours

### 4. Performance/Stress Tests (TODO - minutes/hours) ⏳ NOT IMPLEMENTED

**Duration**: Long-running (minutes to hours)  
**Hardware**: GPS module running continuously

**Tests needed**:
- ⏳ Long-term stability (24+ hours)
- ⏳ Jitter accumulation over time
- ⏳ Memory leak detection
- ⏳ Thread safety under load
- ⏳ Signal loss and recovery
- ⏳ GPS restart handling

## Recommendations

### For Development/CI

**Current approach is correct**:
- ✅ Use fast API tests (0.03s) for CI pipeline
- ✅ Test error handling without hardware
- ✅ Validate thread safety and structures
- ✅ Run on every commit

**Add hardware tests separately**:
- ⏳ Mark as `[hardware]` or `[slow]` tests
- ⏳ Run only when hardware available
- ⏳ Exclude from standard CI pipeline
- ⏳ Run manually or on hardware test rig

### For Hardware Validation

**Create dedicated test suite**:
```cmake
# Fast API tests (always run)
add_test(NAME test_pps_detector COMMAND test_pps_detector)
set_tests_properties(test_pps_detector PROPERTIES
    LABELS "gps;pps;unit;fast"
    TIMEOUT 5  # Very short timeout for API tests
)

# Slow hardware tests (manual/hardware-only)
add_test(NAME test_pps_hardware COMMAND test_pps_hardware)
set_tests_properties(test_pps_hardware PROPERTIES
    LABELS "gps;pps;hardware;slow"
    TIMEOUT 30  # Realistic timeout for hardware
)

# Run only fast tests by default
ctest -L fast

# Run hardware tests explicitly
ctest -L hardware
```

### For Integration Testing

**Staged approach**:
1. **Phase 1** (Current): API tests without hardware ✅
2. **Phase 2** (Next): Hardware validation with real GPS ⏳
3. **Phase 3** (Future): Integration with PTP time converter ⏳
4. **Phase 4** (Long-term): Performance and stress testing ⏳

## Conclusion

### Current Status

✅ **API Unit Tests**: Complete and working (0.03s)
- Validates software correctness
- No hardware required
- Suitable for CI pipeline

⏳ **Hardware Tests**: Not yet implemented
- Requires real GPS module
- Takes 2-5+ seconds minimum
- Needed before production use

### Key Takeaways

1. **0.03s test time is CORRECT for API tests** - it tests software, not hardware
2. **Real PPS detection takes 2-5+ seconds** - this is by design (3 edges @ 1Hz)
3. **Current tests are valuable** - they catch API bugs, initialization errors, thread safety issues
4. **Hardware tests are needed next** - to validate real PPS signal detection
5. **Test timing expectations must be clear** - API tests ≠ hardware validation

### Next Steps

1. **Document test strategy** (this document) ✅
2. **Update test output** to clarify API-only nature ✅
3. **Create hardware test suite** (test_pps_hardware.cpp) ⏳
4. **Integrate with GPS time converter** ⏳
5. **Validate with real u-blox NEO-G7** ⏳

---

**Document Version**: 1.0  
**Date**: November 13, 2025  
**Status**: API tests complete, hardware tests pending  
**Author**: Development Team
