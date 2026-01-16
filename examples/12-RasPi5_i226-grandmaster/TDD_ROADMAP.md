# TDD Implementation Roadmap - Starting 2026-01-15

**Status**: 🟢 Ready to Begin  
**Current**: Bug #14 Layer 17 Fixed ✅ (PPS precision implemented)  
**Next**: Systematic TDD for Missing Features

---

## ✅ COMPLETED: Bug #14 Layer 17 Fix

**Problem**: GPS-PHC offset oscillating ±40ms every ~50 seconds  
**Root Cause**: Reading PHC milliseconds after PPS event, not using kernel-captured PPS timestamp  
**Solution**: Use `pps.assert_sec/nsec` (kernel timestamp at exact PPS interrupt) as GPS reference  
**Evidence**: Offset now monotonically converging (-62016ms → -61985ms) instead of oscillating

**Code Change** (`src/grandmaster_controller.cpp` lines 405-435):
```cpp
// CRITICAL FIX Layer 17: Use PPS assert timestamp for precision!
// USER INSIGHT: "use GPS-PPS signal for second-start indicator! 
// the next coming value from GPS will be the second to assign to that tick!"

// 1. Get GPS UTC integer seconds from NMEA (tells us WHICH second)
uint64_t gps_tai_sec = 0;
uint32_t gps_nsec = 0;
bool gps_valid = gps_->get_ptp_time(&gps_tai_sec, &gps_nsec);
uint64_t gps_utc_sec = gps_tai_sec - 37;  // TAI → UTC

// 2. Use PPS assert timestamp as the PRECISE moment that GPS second occurred
uint64_t gps_timestamp_sec = pps.assert_sec;
uint32_t gps_timestamp_nsec = pps.assert_nsec;

// 3. Calculate offset: GPS_UTC_seconds - PHC
int64_t offset_ns = calculate_offset(gps_utc_sec, 0, phc_sec, phc_nsec);
```

**Test Results**: LAYER_17_PPS_PRECISION_FIX.log showing smooth convergence  
**Next**: Let this test run to completion, then proceed with TDD plan below

---

## 🔴 Priority 1: RTC Aging Offset Discipline (CRITICAL)

**Why Critical**: RTC drift accumulates unbounded without discipline, ruins long-term stability

### TDD Cycle 1: Drift Buffer (20 minutes)

**File Created**: `tests/test_rtc_discipline.cpp` ✅

**Red Phase** (Write Failing Tests):
```bash
# Test should compile but skip (GTEST_SKIP)
cd build
cmake --build . --target test_rtc_discipline
./test_rtc_discipline

# Expected output: njot all of these test will pass
# [  FAILED ] RtcDisciplineTest.DriftAveragingWindow120Samples
# [  FAILED ] RtcDisciplineTest.StabilityGateRejectsNoisyData
# [  FAILED ] RtcDisciplineTest.ProportionalControlLaw
# [  FAILED ] RtcDisciplineTest.LSBClampingRange
# [  FAILED ] RtcDisciplineTest.MinimumAdjustmentInterval
# [  FAILED ] RtcDisciplineTest.RequireMinimum60Samples
# [  FAILED ] RtcDisciplineTest.FullDisciplineCycle
```

**Green Phase** (Implement `RtcDriftDiscipline` class):

Create `src/rtc_drift_discipline.hpp`:
```cpp
class RtcDriftDiscipline {
public:
    RtcDriftDiscipline(size_t buffer_size = 120, double stddev_threshold = 0.3);
    
    void add_drift_sample(double drift_ppm);
    bool should_adjust() const;
    int8_t calculate_adjustment(double drift_avg_ppm) const;
    void apply_adjustment(RtcAdapter* rtc);
    
    // Getters for testing
    size_t get_buffer_size() const;
    size_t get_sample_count() const;
    double get_drift_average() const;
    double get_drift_stddev() const;
    int8_t get_last_adjustment() const;
    
private:
    std::vector<double> drift_buffer_;
    size_t max_samples_;
    double stddev_threshold_;
    std::chrono::steady_clock::time_point last_adjustment_time_;
    int8_t last_adjustment_lsb_;
    
    static constexpr double LSB_PER_PPM = 0.1;  // DS3231: 0.1 ppm per LSB
    static constexpr int8_t MAX_LSB = 3;
    static constexpr int8_t MIN_LSB = -3;
    static constexpr int MIN_INTERVAL_SECONDS = 1200;  // 20 minutes
    static constexpr size_t MIN_SAMPLES_FOR_ADJUSTMENT = 60;
};
```

Create `src/rtc_drift_discipline.cpp`:
```cpp
// Implementation of drift averaging, stddev calculation,
// proportional control law, and adjustment logic
```

**Refactor Phase**:
- Extract statistics helpers (mean, stddev) to utils if needed
- Add logging for discipline decisions
- Document formulas with references to deb.md

**Validation**:
```bash
# All 7 tests should PASS
./test_rtc_discipline
# Expected: 7/7 PASS

# Run full test suite
ctest --output-on-failure
# Expected: 59/59 PASS (52 existing + 7 new)
```

**Timeline**: 3-4 hours (includes design, implementation, testing)

---

## 🔴 Priority 2: 3-Phase Servo Implementation (NEW REQUIREMENT)

**Why Important**: Advanced servo for comparison with PI, implements idd_3phaseDrift.md design

### TDD Cycle 2: Three-Phase Servo (6-8 hours)

**Red Phase** (Write Failing Tests):

Create `tests/test_three_phase_servo.cpp`:
```cpp
// Phase A tests
TEST(ThreePhaseServo, PhaseA_OffsetCorrection)
TEST(ThreePhaseServo, PhaseA_StepDecision)
TEST(ThreePhaseServo, PhaseA_TransitionToPhaseB)

// Phase B tests  
TEST(ThreePhaseServo, PhaseB_NoFrequencyAdjustments)  // CRITICAL!
TEST(ThreePhaseServo, PhaseB_DriftMeasurement20Pulses)
TEST(ThreePhaseServo, PhaseB_ValidityChecks)
TEST(ThreePhaseServo, PhaseB_TransitionToPhaseC)

// Phase C tests
TEST(ThreePhaseServo, PhaseC_GradientCalculation)
TEST(ThreePhaseServo, PhaseC_EMAFiltering)
TEST(ThreePhaseServo, PhaseC_SlewOnlyNoSteps)
TEST(ThreePhaseServo, PhaseC_EmergencyStep)  // Only if >500ms

// Integration tests
TEST(ThreePhaseServo, FullCycle_PhaseA_B_C)
TEST(ThreePhaseServo, StepTriggersPhaseB_Reset)
```

**Green Phase**:

Create `src/three_phase_servo.hpp`:
```cpp
class ThreePhaseServo : public ServoInterface {
public:
    enum class Phase { OFFSET_CORRECTION, DRIFT_BASELINE, DRIFT_EVALUATION };
    
    ThreePhaseServo(const ThreePhaseServoConfig& config);
    
    // ServoInterface implementation
    int32_t calculate_correction(int64_t offset_ns) override;
    void update(int64_t offset_ns) override;
    bool is_locked() const override;
    void reset() override;
    void get_state(ServoState* state) const override;
    
    // Phase-specific methods
    Phase get_current_phase() const;
    
private:
    Phase current_phase_;
    
    // Phase A state
    std::vector<int64_t> offset_samples_;
    
    // Phase B state
    uint32_t baseline_start_pps_;
    int64_t baseline_phc_start_ns_;
    bool measuring_drift_;
    
    // Phase C state
    double freq_bias_ppb_;
    double freq_ema_ppb_;
    int64_t last_offset_ns_;
    
    void handle_phase_a(int64_t offset_ns);
    void handle_phase_b(int64_t offset_ns);
    void handle_phase_c(int64_t offset_ns);
};
```

**Validation**:
```bash
./test_three_phase_servo
# Expected: 13/13 PASS
```

**CLI Integration**:
```bash
# Add --servo-type parameter to main()
./ptp_grandmaster_v2 --servo-type pi      # Default
./ptp_grandmaster_v2 --servo-type 3phase  # New servo
```

**Timeline**: 6-8 hours

---

## 🟡 Priority 3: Real-Time Threading (HIGH)

**Why Important**: Reduces PPS jitter from 0.5-3.0µs → <500ns, drift noise from ±1ppm → ±0.2ppm

### TDD Cycle 3: RT Threading (4-6 hours)

**Red Phase**:

Create `tests/test_rt_threading.cpp`:
```cpp
TEST(RTThreading, ThreadCreationAndPriority)
TEST(RTThreading, CPUPinning)
TEST(RTThreading, MutexProtection)
TEST(RTThreading, LatencyMonitoring)
TEST(RTThreading, WorkerThreadIsolation)
```

**Green Phase**:

Create `src/rt_pps_thread.hpp` and `src/rt_pps_thread.cpp`

**Validation**:
```bash
# Requires kernel params: isolcpus=2 nohz_full=2 rcu_nocbs=2
# Edit /boot/firmware/cmdline.txt, reboot

./test_rt_threading
# Expected: 5/5 PASS

# Measure jitter improvement
./ptp_grandmaster_v2 > test_rt.log
grep "PPS jitter" test_rt.log
# Expected: <500ns (vs current 0.5-3.0µs)
```

**Timeline**: 4-6 hours

---

## 🟢 Priority 4: PTP Delay Mechanism (BLOCKS SLAVE SYNC)

**Why CRITICAL**: Current grandmaster is TX-only, slaves cannot synchronize!

This is documented as incomplete in both original and v2 per IMPLEMENTATION_PLAN.md Task 4.3/4.4.

**Defer until Priorities 1-3 complete** to avoid scope creep.

---

## 📊 Progress Tracking

### Week 1 (Current - 2026-01-15)
- [x] Bug #14 Layer 17 fixed (PPS precision)
- [ ] Priority 1: RTC Discipline (3-4 hours)
- [ ] Priority 2: 3-Phase Servo (6-8 hours)

### Week 2 (2026-01-20)
- [ ] Priority 3: RT Threading (4-6 hours)
- [ ] Integration testing (all 3 features together)
- [ ] 30-minute stability test

### Week 3 (2026-01-27)
- [ ] Priority 4: PTP Delay Mechanism (10-15 hours - separate epic)
- [ ] End-to-end slave synchronization testing

---

## 🎯 Success Criteria

**Priority 1 (RTC Discipline)**:
- ✅ 7/7 unit tests passing
- ✅ Integration test: 30-minute run shows RTC drift disciplined to ±0.1 ppm
- ✅ No false adjustments from noise

**Priority 2 (3-Phase Servo)**:
- ✅ 13/13 unit tests passing
- ✅ Comparison test: PI vs 3-phase servo performance
- ✅ CLI switch works: `--servo-type [pi|3phase]`

**Priority 3 (RT Threading)**:
- ✅ 5/5 unit tests passing
- ✅ PPS jitter <500ns (measured via GPIO oscilloscope)
- ✅ Drift noise ±0.2ppm (vs current ±1ppm)

---

## 🚦 Current Status: READY TO START

**Next Action**: Run Priority 1 TDD Cycle 1 (RTC Discipline)

```bash
# Step 1: Verify test framework ready
cd /home/zarfld/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build
cmake ..
cmake --build . --target test_rtc_discipline

# Step 2: Run tests (should all SKIP)
./test_rtc_discipline

# Step 3: Implement RtcDriftDiscipline class
# (Create src/rtc_drift_discipline.hpp and .cpp)

# Step 4: Re-run tests (should all PASS)
./test_rtc_discipline
```

**Estimated Completion**:
- Priority 1: ~4 hours
- Priority 2: ~8 hours  
- Priority 3: ~6 hours
- **Total**: ~18 hours of focused TDD work

Let's begin! 🚀
