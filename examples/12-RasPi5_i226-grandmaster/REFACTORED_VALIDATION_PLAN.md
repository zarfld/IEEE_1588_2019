# Refactored Executable Validation Plan

**Date**: 2026-01-14  
**Purpose**: Verify ptp_grandmaster_v2 has all functionality from original  
**Approach**: TDD + Integration Testing + Hardware Validation

---

## 📊 Feature Comparison Matrix

**IMPORTANT**: Original ptp_grandmaster.cpp is **~50% complete** (status per IMPLEMENTATION_PLAN.md). Both versions are incomplete!

| Feature | Original ptp_grandmaster | Refactored v2 | Planned (IMPLEMENTATION_PLAN.md) | Status | Priority |
|---------|-------------------------|---------------|----------------------------------|--------|----------|
| **Core GPS/PPS** | | | | | |
| GPS NMEA parsing | ✅ | ✅ (GpsAdapter) | ✅ COMPLETE | PASS | - |
| PPS timestamp capture | ✅ | ✅ (GpsAdapter) | ✅ COMPLETE | PASS | - |
| PPS dropout detection | ✅ (dropout_detected, seq_delta) | ✅ (GpsAdapter) | ✅ COMPLETE | PASS | - |
| **RTC Discipline** | | | | |
| DS3231 I2C access | ✅ | ✅ (RtcAdapter) | PASS | Unit tested |
| Aging offset control | ✅ | ❌ **MISSING** | ⚠️ FAIL | 🔴 Critical |
| Drift averaging (120 samples) | ✅ | ❌ **MISSING** | ⚠️ FAIL | 🔴 Critical |
| Proportional control law | ✅ (delta_lsb calculation) | ❌ **MISSING** | ⚠️ FAIL | 🔴 Critical |
| Stability gate (stddev < 0.3ppm) | ✅ | ❌ **MISSING** | ⚠️ FAIL | 🔴 Critical |
| **Servo State Machine** | | | | |
| Three states (LOCKED_GPS/HOLDOVER_RTC/RECOVERY_GPS) | ✅ | ✅ (ServoStateMachine) | PASS | Unit tested (10/10) |
| Lock detection (±100ns phase AND ±5ppb freq) | ✅ | ✅ (lines 22-23) | PASS | Unit tested |
| Stability counter (10 samples) | ✅ | ✅ | PASS | Unit tested |
| GPS loss → HOLDOVER transition | ✅ | ✅ | PASS | Unit tested |
| GPS recovery → LOCKED transition | ✅ | ✅ | PASS | Unit tested |
| **PHC Calibration** | | | | |
| 20-pulse frequency measurement | ✅ | ✅ (PhcCalibrator) | PASS | Unit tested (7/7) |
| Drift measurement (ppm) | ✅ | ✅ | PASS | Unit tested |
| Sanity check (reject >2000ppm) | ✅ | ✅ | PASS | Unit tested |
| Iterative convergence | ✅ | ✅ | PASS | Unit tested |
| **Servo Engines** | | | | |
| PI servo (phase-based) | ✅ | ✅ (PI_Servo) | PASS | Unit tested (10/10) |
| Anti-windup | ✅ | ✅ | PASS | Unit tested |
| Lock detection | ✅ | ✅ | PASS | Unit tested |
| 3-Phase Servo (idd_3phaseDrift.md) | ❌ **NOT IN ORIGINAL** | ❌ **MISSING** | ⚠️ FAIL | 🔴 **NEW REQUIREMENT** |
|  ├─ Phase A: Offset Correction | N/A | ❌ | ⚠️ FAIL | Step once, reset baseline |
|  ├─ Phase B: Drift Baseline (NO ADJ!) | N/A | ❌ | ⚠️ FAIL | 20 PPS pure measurement |
|  ├─ Phase C: Drift Evaluation (df/dt) | N/A | ❌ | ⚠️ FAIL | EMA filter, slew only |
| Servo type CLI switch (--servo-type) | N/A | ❌ **MISSING** | ⚠️ FAIL | 🟡 High priority |
| **Real-Time Threading** | | | | |
| RT thread (SCHED_FIFO 80) | ✅ | ❌ **MISSING** | ⚠️ FAIL | 🟡 High |
| CPU pinning (isolcpus=2) | ✅ | ❌ **MISSING** | ⚠️ FAIL | 🟡 High |
| Mutex-protected shared data | ✅ | ❌ **MISSING** | ⚠️ FAIL | 🟡 High |
| Latency monitoring (<10ms) | ✅ | ❌ **MISSING** | ⚠️ FAIL | 🟡 High |
| **Network/PTP Messages** | | | | | |
| Hardware timestamping (TX) | ✅ | ✅ (NetworkAdapter) | ✅ COMPLETE | PASS | - |
| Multicast join | ✅ | ✅ | ✅ COMPLETE | PASS | - |
| Sync message TX | ✅ | ✅ | ✅ COMPLETE | PASS | - |
| Announce message TX | ✅ | ✅ | ✅ COMPLETE | PASS | - |
| Follow_Up message TX | ✅ | ✅ | ✅ COMPLETE | PASS | - |
| **RECEIVE Path (CRITICAL MISSING)** | ❌ | ❌ | ⏳ **PLANNED** | ⚠️ **FAIL** | 🔴 **CRITICAL** |
| RX message parsing | ❌ | ❌ | ⏳ Task 4.3 INCOMPLETE | ⚠️ **FAIL** | 🔴 **CRITICAL** |
| Hardware timestamping (RX) | ❌ | ❌ | ⏳ 60% HAL done | ⚠️ **FAIL** | 🔴 **CRITICAL** |
| Delay_Req reception | ❌ | ❌ | ⏳ Task 4.3 MISSING | ⚠️ **FAIL** | 🔴 **CRITICAL** |
| Delay_Resp transmission | ❌ | ❌ | ⏳ Task 4.3 MISSING | ⚠️ **FAIL** | 🔴 **CRITICAL** |
| Event loop RX integration | ❌ | ❌ | ⏳ Task 4.1 MISSING | ⚠️ **FAIL** | 🔴 **CRITICAL** |
| **RESULT**: Slaves cannot sync! | ❌ | ❌ | **BLOCKS VALIDATION** | ⚠️ **FAIL** | 🔴 **CRITICAL** |

---

## 🚨 Critical Missing Features (From IMPLEMENTATION_PLAN.md)

### 0. PTP Delay Mechanism 🔴 **BLOCKING CRITICAL** (Neither version has this!)
**Original Status**: ❌ NOT IMPLEMENTED (Task 4.3, 4.4 incomplete)
**Refactored Status**: ❌ NOT IMPLEMENTED
**Impact**: **SLAVES CANNOT SYNCHRONIZE** - Grandmaster is transmit-only!

**Missing from BOTH versions** (IMPLEMENTATION_PLAN.md lines 680-750):
- ❌ Receive incoming PTP messages (Delay_Req from slaves)
- ❌ Parse Delay_Req messages
- ❌ Calculate propagation delay
- ❌ Transmit Delay_Resp messages with timestamps
- ❌ Hardware RX timestamping (HAL 60% done, RX incomplete)
- ❌ Event loop integration for message reception

**This is THE critical gap preventing any PTP slave from using this grandmaster!**

**Required Implementation** (per IMPLEMENTATION_PLAN.md Task 4.3):
```cpp
// Task 4.3: Message Reception - ⏳ MISSING
- [ ] Poll event socket for incoming messages
- [ ] Extract RX hardware timestamps from MSG_ERRQUEUE
- [ ] Parse Delay_Req messages using repository types
- [ ] Validate message integrity (CRC, sequence, domain)

// Task 4.4: Delay Response - ⏳ MISSING
- [ ] Calculate requestReceiptTimestamp (RX timestamp of Delay_Req)
- [ ] Construct Delay_Resp message
- [ ] Copy requestingPortIdentity from Delay_Req
- [ ] Transmit Delay_Resp with TX timestamp
- [ ] Log delay request/response pairs
```

**TDD Approach**:
```cpp
// tests/test_ptp_delay_mechanism.cpp (NEW FILE NEEDED)
TEST(PTPDelayMechanism, ReceiveDelayReq) {
    // Test: Parse incoming Delay_Req message
}

TEST(PTPDelayMechanism, ExtractRXTimestamp) {
    // Test: Get RX timestamp from MSG_ERRQUEUE
}

TEST(PTPDelayMechanism, ConstructDelayResp) {
    // Test: Build Delay_Resp with correct timestamps
}

TEST(PTPDelayMechanism, EndToEndDelayCalculation) {
    // Test: Slave can calculate path delay
}
```

---

### 1. RTC Aging Offset Discipline 🔴 CRITICAL
**Original Location**: `ptp_grandmaster.cpp` lines 800-950 (approx.)  
**Original Status**: ✅ IMPLEMENTED (deb.md Recommendation A, E)
**Refactored Location**: ❌ NOT IN RtcAdapter  
**Impact**: RTC drift NOT disciplined, will accumulate unbounded

**Original Implementation**:
- Drift buffer: 120 samples (20 minutes @ 10s intervals)
- Stability gate: stddev < 0.3 ppm threshold  
- Proportional control: `delta_lsb = round(drift_avg_ppm / 0.1)` clamped to [-3, +3]
- Minimum adjustment interval: 1200s (20 minutes)
- Requires 60+ samples before first adjustment

**Required Implementation**:
- [ ] Add drift buffer (120 samples) to RtcAdapter
- [ ] Implement stddev-based stability gate (< 0.3ppm)
- [ ] Implement proportional control: `delta_lsb = round(drift_avg_ppm / 0.1)`
- [ ] Clamp to [-3, +3] LSB range
- [ ] Require 60+ samples before adjustment
- [ ] Minimum 1200s (20 min) between adjustments

**TDD Approach**:
```cpp
// tests/test_rtc_adapter_discipline.cpp
TEST(RtcAdapterDiscipline, DriftAveragingWindow120Samples) {
    // Test: Accumulate 120 samples, verify averaging works
}

TEST(RtcAdapterDiscipline, StabilityGateRejectsNoisyData) {
    // Test: stddev > 0.3ppm → no adjustment
}

TEST(RtcAdapterDiscipline, ProportionalControlLaw) {
    // Test: drift 0.176ppm → delta_lsb = -2
}

TEST(RtcAdapterDiscipline, MinimumAdjustmentInterval) {
    // Test: No adjustment before 1200s elapsed
}
```

### 2. Real-Time Threading 🟡 HIGH PRIORITY
**Original Location**: `ptp_grandmaster.cpp` lines 362-450 (RT thread setup)  
**Original Status**: ✅ IMPLEMENTED (deb.md Recommendation D)
**Refactored Location**: ❌ NOT IN ptp_grandmaster_v2.cpp  
**Impact**: PPS jitter 0.5-3.0µs (vs. target <500ns), drift noise ±1ppm (vs. target ±0.2ppm)

**Original Implementation**:
- RT thread: SCHED_FIFO priority 80, pinned to CPU2
- Worker thread: SCHED_OTHER, pinned to CPU0/1/3
- Mutex-protected shared data: `PpsRtData` struct
- Latency monitoring: warnings if >10ms
- System config required: `isolcpus=2 nohz_full=2 rcu_nocbs=2`

**Required Implementation**:
- [ ] RT thread creation (SCHED_FIFO priority 80)
- [ ] CPU pinning (CPU2 for RT, CPU0/1/3 for worker)
- [ ] Mutex-protected shared data (PpsRtData struct)
- [ ] Latency monitoring and warnings
- [ ] Documentation: kernel boot params `isolcpus=2 nohz_full=2 rcu_nocbs=2`

**TDD Approach**:
```cpp
// tests/test_rt_threading.cpp
TEST(RTThreading, ThreadCreationAndPriority) {
    // Test: Verify SCHED_FIFO priority 80 set correctly
}

TEST(RTThreading, CPUPinning) {
    // Test: Verify RT thread affinity to CPU2
}

TEST(RTThreading, MutexProtection) {
    // Test: Concurrent access to shared data is safe
}

TEST(RTThreading, LatencyMonitoring) {
    // Test: Warnings triggered if latency >10ms
}
```

### 3. Frequency-Error Servo 🟡 HIGH PRIORITY (Step 3)
**Original Status**: ✅ IMPLEMENTED (ptp_grandmaster.cpp lines 1042-1077)  
**Refactored Location**: ❌ NOT IN v2  
**Impact**: MEDIUM - Missing advanced servo for comparison with PI servo

**Original Implementation**:
- Calculates frequency error: `df[n] = (phase_err[n] - phase_err[n-1]) / Δt`
- Applies EMA filter: `freq_ema = 0.3 * df[n] + 0.7 * freq_ema` (alpha=0.3)
- Convergence detection: `|freq_ema| < 1ppb` for 10 consecutive samples
- Parallel implementation alongside PI servo for comparison
- Extended logging every 60 seconds

**Required Implementation**:
- [ ] Add frequency servo state variables to GrandmasterController
- [ ] Implement df/dt calculation in servo loop
- [ ] Implement EMA filtering (alpha=0.3)
- [ ] Add convergence detection logic
- [ ] Add comparison logging (PI vs freq servo outputs)

**TDD Approach**:
```cpp
// tests/test_frequency_error_servo.cpp
TEST(FrequencyErrorServo, CalculateFrequencyError) {
    // Test: df[n] = (phase_err[n] - phase_err[n-1]) / Δt
}

TEST(FrequencyErrorServo, EMAFiltering) {
    // Test: freq_ema = 0.3 * df + 0.7 * freq_ema_prev
}

TEST(FrequencyErrorServo, ConvergenceDetection) {
    // Test: Detect when |freq_ema| < 1ppb for 10 samples
}

TEST(FrequencyErrorServo, ParallelOperation) {
    // Test: Both PI and freq servos run simultaneously
}
```

---

## ✅ Verified Working Features (52 Unit Tests Passing)

| Module | Tests | Status |
|--------|-------|--------|
| PhcAdapter | 7/7 | ✅ PASS |
| PI_Servo | 10/10 | ✅ PASS |
| PhcCalibrator | 7/7 | ✅ PASS |
| ServoStateMachine | 10/10 | ✅ PASS |
| NetworkAdapter | 12/12 | ✅ PASS |
| GrandmasterController | 6/6 | ✅ PASS |

---

## 🧪 Testing Strategy

### Phase 1: Unit Test Gap Analysis (TDD) ✅ CURRENT
**Objective**: Identify missing features via test failures

**Approach**:
1. Write tests for RTC discipline features → FAIL (code doesn't exist)
2. Write tests for RT threading features → FAIL (code doesn't exist)
3. Implement missing features (TDD Red-Green-Refactor)
4. All tests pass → Ready for integration testing

**Timeline**: 2-4 hours  
**Deliverable**: +20 unit tests for missing features

### Phase 2: Integration Testing (Simulated Hardware)
**Objective**: Verify module interactions without real hardware

**Test Cases**:
```cpp
// tests/test_integration_grandmaster.cpp

TEST(Integration, BootSequence) {
    // Test: Initialize → Calibrate → Run loop
    // Verify: All adapters initialize, calibration completes
}

TEST(Integration, GPSLossRecovery) {
    // Test: LOCKED_GPS → (GPS loss) → HOLDOVER_RTC → (GPS return) → RECOVERY_GPS → LOCKED_GPS
    // Verify: State transitions correct, servo freezes during dropout
}

TEST(Integration, RTCDiscipline) {
    // Test: Simulate 120 samples of drift data
    // Verify: Aging offset adjusted correctly
}

TEST(Integration, PTPMessageTransmission) {
    // Test: Controller sends Sync/Announce messages
    // Verify: Network packets transmitted at correct intervals
}
```

**Timeline**: 2-3 hours  
**Deliverable**: Integration test suite (10-15 tests)

### Phase 3: Hardware Validation (Raspberry Pi 5)
**Objective**: Real-world functionality verification

**Test Procedure**:
```bash
# 1. Build refactored executable
cd /home/zarfld/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build
make ptp_grandmaster_v2

# 2. Run with identical arguments to original
sudo ./ptp_grandmaster_v2 --interface eth1 --rtc /dev/rtc1 --rtc-sqw /dev/pps1 --verbose

# 3. Monitor for 30 minutes, capture logs
# Expected:
#   - GPS adapter initializes (finds /dev/ttyACM0 at 38400 baud)
#   - RTC adapter initializes (I2C bus 14, DS3231)
#   - PHC calibration completes (20 pulses, drift 2-7 ppm)
#   - Servo achieves LOCKED_GPS state
#   - RTC drift disciplined (aging offset adjustments logged)
#   - PTP messages transmitted (Sync/Announce)

# 4. Compare behavior with original
diff <(sudo ./ptp_grandmaster ... | tee original.log) \
     <(sudo ./ptp_grandmaster_v2 ... | tee refactored.log)
```

**Success Criteria**:
- ✅ Calibration completes (drift 2-7 ppm)
- ✅ Servo achieves lock (offset <1ms within 5 minutes)
- ✅ Stays in LOCKED_GPS state
- ✅ RTC aging offset adjusts (verify at T+20min, T+40min)
- ✅ No oscillations (zero step corrections after convergence)
- ✅ PTP messages transmitted at 1Hz (Sync) and 0.125Hz (Announce)

**Timeline**: 1 hour (setup + 30min test + analysis)

---

## 📋 Implementation Roadmap

### Iteration 1: Critical RTC Discipline (TDD) 🔴 CRITICAL
**Duration**: 2-3 hours

**Steps**:
1. ✅ Review original RTC discipline code (ptp_grandmaster.cpp lines 800-950)
2. ⏳ Write failing tests for RtcAdapter discipline features
3. ⏳ Implement drift averaging (120-sample buffer)
4. ⏳ Implement stability gate (stddev threshold)
5. ⏳ Implement proportional control law
6. ⏳ Verify all tests pass
7. ⏳ Update RtcAdapter documentation

**Deliverables**:
- `src/rtc_adapter.hpp` (updated interface)
- `src/rtc_adapter.cpp` (discipline implementation)
- `tests/test_rtc_adapter_discipline.cpp` (8-10 tests)

### Iteration 2: RT Threading (TDD) 🟡 HIGH
**Duration**: 2-3 hours

**Steps**:
1. ⏳ Review original RT thread code (ptp_grandmaster.cpp lines 100-300)
2. ⏳ Write failing tests for RT threading features
3. ⏳ Implement RT thread creation in ptp_grandmaster_v2.cpp
4. ⏳ Implement CPU pinning and scheduling policy
5. ⏳ Implement mutex-protected shared data
6. ⏳ Implement latency monitoring
7. ⏳ Verify all tests pass
8. ⏳ Document kernel boot parameter requirements

**Deliverables**:
- `src/ptp_grandmaster_v2.cpp` (RT threading added)
- `tests/test_rt_threading.cpp` (5-7 tests)
- `docs/RT_THREAD_CONFIGURATION.md` (updated)

### Iteration 3: Integration & Hardware Testing ⏳ VALIDATION
**Duration**: 3-4 hours

**Steps**:
1. ⏳ Write integration tests (module interactions)
2. ⏳ Run integration test suite (verify all pass)
3. ⏳ Deploy to Raspberry Pi 5 hardware
4. ⏳ Run 30-minute validation test
5. ⏳ Compare original vs. refactored behavior
6. ⏳ Document any discrepancies
7. ⏳ Fix issues and re-test

**Deliverables**:
- `tests/test_integration_grandmaster.cpp` (10-15 tests)
- `HARDWARE_VALIDATION_REPORT.md` (results + comparison)
- Updated todo list (issues found during testing)

---

## 🎯 Success Metrics

### Unit Testing
- ✅ All component tests pass (52/52 currently)
- ⏳ RTC discipline tests pass (0/10 - to be written)
- ⏳ RT threading tests pass (0/7 - to be written)
- **Target**: 69/69 tests passing (100%)

### Integration Testing
- ⏳ Boot sequence test passes
- ⏳ GPS loss/recovery test passes
- ⏳ RTC discipline test passes
- ⏳ PTP message transmission test passes
- **Target**: 10/10 integration tests passing

### Hardware Testing
- ⏳ Calibration completes successfully
- ⏳ Servo achieves lock within 5 minutes
- ⏳ RTC drift disciplined (aging offset adjustments work)
- ⏳ PTP messages transmitted correctly
- ⏳ Behavior matches original ptp_grandmaster
- **Target**: 5/5 validation criteria met

---

## 📝 Current Status (2026-01-14 16:50 UTC)

### ✅ Recently Completed
- **Bug #14: PHC Timescale Tracking** - Implemented cumulative step tracking with PPS timestamp adjustment
- **I2C Bus Auto-Detection** - Dynamic detection via sysfs + I2C scanning, works across reboots
- **GPS Lock Validation** - Wait for PPS-UTC lock before initial time set and step corrections

### 🏃 In Progress
- **Extended Operation Test** - Running ptp_grandmaster_v2 for 30+ minutes to verify Bug #14 fix prevents continuous stepping
  - Started: 2026-01-14 16:49 UTC
  - Status: Initializing (waiting for GPS PPS-UTC lock)
  - Expected: Lock within 30s, then calibration and main loop

### 📋 Next Immediate Actions

**Priority 1: Complete Extended Operation Test** 🔴
Monitor for:
- GPS PPS-UTC lock established (should occur within 30s)
- PHC calibration completes (20 pulses, drift measurement)
- Servo achieves LOCKED_GPS state
- **Critical: No continuous step corrections** (Bug #14 verification)
- Offset converges to <1ms within 5 minutes
- System remains stable in main loop

**Priority 2: PTP Delay Mechanism** 🔴 **BLOCKS SLAVE SYNC**
- [ ] Implement RX message parsing (Delay_Req reception)
- [ ] Extract RX hardware timestamps from MSG_ERRQUEUE
- [ ] Implement Delay_Resp message construction and transmission
- [ ] Test end-to-end delay calculation
- **Impact**: Currently slaves CANNOT synchronize to this grandmaster

**Priority 3: RTC Aging Offset Discipline** 🔴
- [ ] Add 120-sample drift buffer to RtcAdapter
- [ ] Implement stddev-based stability gate (<0.3ppm)
- [ ] Implement proportional control law: `delta_lsb = round(drift_avg_ppm / 0.1)`
- [ ] Test with simulated drift data

**Priority 4: Real-Time Threading** 🟡
- [ ] RT thread creation (SCHED_FIFO priority 80)
- [ ] CPU pinning (CPU2 for RT, CPU0/1/3 for worker)
- [ ] Mutex-protected shared data
- [ ] Latency monitoring and warnings

### 📊 Test Results Summary

**System Initialization** (from extended test run 2026-01-14 15:50 UTC):
```
✅ GPS adapter: 38400 baud, NMEA mode detected
✅ RTC adapter: I2C bus 13 auto-detected, SQW ±1µs precision
✅ PHC adapter: eth1 → /dev/ptp0, max_adj 62499999 ppb
✅ Network adapter: HW timestamping enabled
✅ GPS fix: 5-10 satellites acquired
✅ GPS PPS-UTC lock: Established at PPS seq #4179 (after initial steps)
✅ PHC calibration: Completed in 3 iterations (final drift: -432756 ppb)
✅ Servo state: Entered LOCKED_GPS state
```

**🚨 CRITICAL BUG DISCOVERED - Bug #14 NOT FULLY FIXED!**

**Symptom**: Continuous step corrections every 3-8 seconds despite PPS timestamp adjustment
```
cumulative_phc_steps_ns: 0.96s → 1.86s → 3.66s → 5.26s → ... → 102.43s (after 2 minutes)
```

**Evidence from test run**:
```
[Controller] Loop 5: GPS=1768405939.317779330
[Controller] Applying step correction (offset > 100 ms)
[GpsAdapter] PHC stepped by 0.947726065s, cumulative=0.960308977s

[Controller] Loop ~10: GPS=1768405943.6695278
[Controller] Applying step correction (offset > 100 ms)
[GpsAdapter] PHC stepped by 0.899798625s, cumulative=1.860107602s

[Controller] Loop ~15: GPS=1768405947.555103224
[Controller] Applying step correction (offset > 100 ms)
[GpsAdapter] PHC stepped by 1.802234789s, cumulative=3.662342391s

... continues indefinitely ...

[Controller] Loop ~150: GPS=1768406106.802217682
[Controller] Applying step correction (offset > 100 ms)
[GpsAdapter] PHC stepped by 7.184351113s, cumulative=102.431354951s
```

**Root Cause Analysis**:

**FOUND THE BUG!** 🎯

The PPS timestamp adjustment in `get_ptp_time()` is **incorrectly applied**. Here's what's happening:

```cpp
// gps_adapter.cpp lines 876-895
uint64_t utc_sec = base_utc_sec_ + (pps_data_.sequence - base_pps_seq_);  // ✅ CORRECT
*seconds = utc_sec + TAI_UTC_OFFSET;  // ✅ CORRECT

// 🔴 BUG: Adjusting PPS nanoseconds by cumulative PHC steps
int64_t adjusted_nsec = static_cast<int64_t>(pps_data_.assert_nsec) + cumulative_phc_steps_ns_;
*nanoseconds = static_cast<uint32_t>(adjusted_nsec);  // ❌ WRONG!
```

**Problem**: `cumulative_phc_steps_ns_` keeps growing (0.96s → 102s+), so the nanoseconds field becomes massively offset from the actual PPS timestamp. This creates a huge time error that triggers continuous stepping.

**Example**:
- PPS pulse arrives at PHC time: 1768405939.300000000
- After PHC step: cumulative_phc_steps_ns = 960308977 ns (≈0.96s)
- **Incorrect** time returned: 1768405939 seconds + (300000000 + 960308977) ns = 1768405940.260308977
- **Actual** GPS time should be: 1768405939.300000000 (from NMEA + PPS mapping)
- **Error**: 0.960308977 seconds → triggers step correction!

**Why This Approach is Wrong**:

The original understanding was: "PPS timestamps are PHC hardware captures, so after PHC steps, adjust them."

BUT: The GPS time returned by `get_ptp_time()` should be **UTC time from NMEA + PPS**, NOT the PHC hardware timestamp!

**Correct Approach**:

The GPS adapter should return **NMEA-derived UTC time** based on the BASE MAPPING:
- UTC seconds: `base_utc_sec_ + (pps_seq - base_pps_seq_)`
- Nanoseconds: The **fractional offset within the second** (NOT the PHC capture time)

Since we're using 1PPS, the nanoseconds should be **close to zero** (the PPS pulse marks the second boundary).

**Correct Fix**:

```cpp
// Return GPS time based on NMEA + PPS mapping
if (base_utc_sec_ > 0) {
    uint64_t utc_sec = base_utc_sec_ + (pps_data_.sequence - base_pps_seq_);
    *seconds = utc_sec + TAI_UTC_OFFSET;
    
    // Nanoseconds: Use PPS assert time as fractional seconds
    // (PPS should be very close to second boundary, typically <1ms jitter)
    *nanoseconds = pps_data_.assert_nsec;
    
    // NO adjustment by cumulative_phc_steps_ns_ here!
    // That adjustment was a misunderstanding of the problem.
    
    return true;
}
```

**Alternative Consideration**:

If the intent was to track PHC timescale for **servo calculations**, then:
- GPS adapter returns pure NMEA+PPS time (no PHC adjustment)
- Controller compares GPS time vs PHC time (both in TAI)
- Servo calculates offset and applies frequency adjustments
- Step corrections only when offset exceeds threshold (and not every cycle!)

**Action Required**:
1. Remove `cumulative_phc_steps_ns_` adjustment from `get_ptp_time()`
2. Test that GPS time remains stable after initial PHC step
3. Verify servo converges without continuous stepping

**Hardware Configuration Verified**:
- Raspberry Pi 5
- Intel i226 NIC (eth1, /dev/ptp0) - igc driver loaded
- u-blox G70xx GPS (/dev/ttyACM0, PPS: /dev/pps0, 38400 baud)
- DS3231 RTC (I2C bus 13, address 0x68, SQW: /dev/pps1)

### ⏭️ What to Watch For

**Success Criteria for Extended Test**:
1. ✅ GPS PPS-UTC lock establishes within 30 seconds
2. ✅ PHC calibration completes successfully (drift 2-7 ppm expected)
3. ✅ Initial PHC step succeeds (no "Invalid argument" errors)
4. ✅ RTC step succeeds
5. ✅ Servo enters LOCKED_GPS state
6. **🔴 CRITICAL**: No continuous stepping after initial convergence (Bug #14 verification)
7. ✅ Offset reduces to <1ms within 5 minutes
8. ✅ System remains stable, no oscillations
9. ✅ PTP Sync/Announce messages transmitted at correct intervals

**Failure Indicators**:
- ❌ GPS PPS-UTC lock fails after 30s (time unreliable)
- ❌ PHC step failures ("Invalid argument" errors)
- ❌ Continuous step corrections every cycle (Bug #14 regression)
- ❌ Offset does not converge or oscillates
- ❌ Servo state machine thrashing
- ❌ Segfaults or crashes
