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
| Frequency-error servo (df/dt) | ✅ **IMPLEMENTED** (lines 1042-1077) | ❌ **MISSING** | ⚠️ FAIL | 🟡 High priority |
| EMA filter (freq_ema) | ✅ **IMPLEMENTED** (alpha=0.3) | ❌ **MISSING** | ⚠️ FAIL | 🟡 High priority |
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

## 📝 Next Immediate Action

**Start with TDD for RTC Discipline** (most critical missing feature):

```bash
# 1. Create test file
touch tests/test_rtc_adapter_discipline.cpp

# 2. Write first failing test
# TEST(RtcAdapterDiscipline, DriftAveragingWindow120Samples)

# 3. Run test (should FAIL)
cd build && make test_rtc_adapter_discipline && ./test_rtc_adapter_discipline

# 4. Implement minimal code to pass
# Update src/rtc_adapter.cpp with drift buffer

# 5. Re-run test (should PASS)
./test_rtc_adapter_discipline

# 6. Refactor and repeat for next feature
```

**Question for User**: Should we start with:
- **Option A**: TDD for RTC discipline (most critical functionality gap)
- **Option B**: Run hardware test now and document what's missing
- **Option C**: Write integration tests first to understand interactions

Which approach do you prefer? 🤔
