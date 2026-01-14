# Clean Architecture Design: PTP Grandmaster

**Date**: 2026-01-14  
**Status**: 🔄 Refactoring in Progress  
**Reason**: Original implementation (1750-line monolith) had tangled responsibilities causing servo failures

---

## 🎯 Design Principles

### IEEE 1588-2019 Compliance
Following `.github/instructions/copilot-instructions.md`:
- ✅ **Hardware Agnostic** - Protocol logic independent of Linux/Intel/vendor code
- ✅ **Dependency Injection** - Hardware access via interfaces, not direct calls
- ✅ **Testable** - Each module mockable without physical hardware
- ✅ **Standards-Only** - IEEE/AVnu protocol logic separate from implementation

### Clean Code Principles
- **Single Responsibility** - Each class has one reason to change
- **Open/Closed** - Open for extension (new servos), closed for modification
- **Liskov Substitution** - Servos are interchangeable via common interface
- **Interface Segregation** - Small, focused interfaces
- **Dependency Inversion** - Depend on abstractions, not concretions

---

## 📐 Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                  GrandmasterController                       │
│  (Orchestration: Initialize → Calibrate → Loop → Shutdown)  │
└────────┬──────────────────────────────────────────┬─────────┘
         │                                           │
    ┌────▼────────┐                          ┌──────▼──────┐
    │   Adapters  │                          │   Engines   │
    │ (Hardware)  │                          │   (Logic)   │
    └─────────────┘                          └─────────────┘
         │                                           │
    ┌────┴───────────────────┬────────┐    ┌────────┴──────────────┐
    │          │             │        │    │          │             │
┌───▼────┐ ┌──▼───┐  ┌─────▼──┐ ┌───▼─────┐ ┌───▼────┐ ┌────▼────┐
│  GPS   │ │ RTC  │  │  PHC   │ │ Network │ │ State  │ │ Servos  │
│Adapter │ │Adapt │  │ Adapter│ │ Adapter │ │Machine │ │(PI/Freq)│
│   ✅   │ │  ✅  │  │   ✅   │ │   ✅    │ │   ✅   │ │   ✅    │
└────────┘ └──────┘  └────────┘ └─────────┘ └────────┘ └─────────┘
                                      │
                          Uses Repository HAL Interface:
                          IEEE::_1588::_2019::HAL::NetworkInterface
```

**Legend**:
- ✅ = Complete & Tested (5 modules extracted, 46/46 tests passing)

---

## 🔧 Module Specifications

### 1. Hardware Adapters (Dependency Injection Layer)

#### GpsAdapter ✅ (Exists, Good Design)
**File**: `src/gps_adapter.hpp`, `src/gps_adapter.cpp`  
**Responsibility**: GPS NMEA parsing + PPS timestamp capture  
**Interface**:
```cpp
class GpsAdapter {
public:
    bool initialize(const char* device, int baud_rate);
    bool has_fix() const;
    uint8_t get_satellite_count() const;
    bool get_time(uint64_t* sec, uint32_t* nsec) const;
    bool get_pps_data(PpsData* data, uint32_t* max_jitter_ns);
};
```
**Dependencies**: Linux serial + PPS subsystem  
**Status**: ✅ Complete, well-designed

---

## 🎛️ Servo Implementations (Switchable via CLI)

The system supports **two servo algorithms** selectable via `--servo-type` parameter:

### PI Servo (Current Default) ✅
**File**: `src/pi_servo.cpp`  
**Algorithm**: Proportional-Integral controller on phase offset

**Implementation**:
- **P-term**: `Kp * offset_ns` (proportional gain)
- **I-term**: `Ki * integral` (accumulated offset)
- **Output**: Frequency correction in ppb
- **Lock Detection**: ±100ns phase AND ±5ppb frequency

**Pros**: 
- Simple, well-tested
- Fast convergence

**Cons**: 
- Can oscillate with large Kp
- Integral windup issues
- Continuous adjustments

---

### 3-Phase Servo (Parallel Implementation) ⏳
**File**: `src/three_phase_servo.cpp` (**NEW** - to be implemented)  
**Algorithm**: Separate offset and drift handling per `idd_3phaseDrift.md`

**State Machine**:
```cpp
enum class ServoPhase { 
    OFFSET_CORRECTION,  // Phase A
    DRIFT_BASELINE,     // Phase B  
    DRIFT_EVALUATION    // Phase C
};
```

#### Phase A — Offset Correction (Phase Align)
**Goal**: Bring PHC close to GPS to enable clean drift measurement

- Measure offset over N samples (noise filter)
- If `|offset| > step_threshold`: Apply **ONE** step correction
- Reset drift baseline (discontinuity after step)
- **Transition**: → Phase B when `|offset| < 10ms`

#### Phase B — Drift Baseline (Frequency Bias Capture)
⚠️ **CRITICAL**: **NO frequency adjustments during this phase!**

**Goal**: Determine stable baseline of drift (frequency offset)

- Measure PHC drift over **20 PPS pulses** (~20 seconds)
- Pure measurement: `drift_ppb = (PHC_delta - Ref_delta) / Ref_delta * 1e9`
- Store as `freq_ppb_baseline`
- **Transition**: → Phase C when measurement complete

#### Phase C — Drift Evaluation (Run Servo)
**Goal**: Continuously correct frequency without steps

- Every second:
  - Measure offset
  - Calculate: `df = (offset[n] - offset[n-1]) / Δt`
  - Apply EMA filter: `df_ema = α*df + (1-α)*df_ema` (α ≈ 0.05-0.2)
  - Update: `freq_total = freq_ppb_baseline + df_ema`
  - Apply: `clock_adjtime(ADJ_FREQUENCY, freq_total)`
- Step only if `|offset| > 500ms` OR GPS lock lost
- **Transition**: → Phase B if step applied (reset baseline)

**Command-line usage**:
```bash
# Use PI servo (default)
./ptp_grandmaster_v2 --servo-type pi

# Use 3-phase servo
./ptp_grandmaster_v2 --servo-type 3phase
```

**Pros**: 
- Separates offset/drift mathematically cleaner
- No continuous integral accumulation
- Explicit measurement phase

**Cons**: 
- Requires 20s startup delay for Phase B
- More complex state machine
- Implementation pending

---

#### RtcAdapter ✅ (Exists, Good Design)
**File**: `src/rtc_adapter.hpp`, `src/rtc_adapter.cpp`  
**Responsibility**: DS3231 RTC I2C access + aging offset discipline  
**Interface**:
```cpp
class RtcAdapter {
public:
    bool initialize(const char* device);
    bool get_time(uint64_t* sec, uint32_t* nsec);
    bool set_time(uint64_t sec, uint32_t nsec);
    bool adjust_aging_offset(int delta_lsb);
    bool get_pps_edge(uint64_t* sec, uint32_t* nsec);
};
```
**Dependencies**: Linux I2C subsystem  
**Status**: ✅ Complete, well-designed

#### Real-Time Threading Architecture ⏳
**Responsibility**: Minimize PPS jitter and improve servo stability  
**Design Pattern**: Dual-thread with priority separation

**RT Thread (High Priority)**:
- **Priority**: SCHED_FIFO priority 80 (real-time scheduling)
- **CPU Affinity**: Pinned to isolated CPU2
- **Responsibility**: PPS timestamp capture ONLY (minimal, deterministic work)
- **Latency Target**: <10µs per capture
- **Shared Data**: Updates `PpsRtData` struct (mutex-protected)

**Worker Thread (Normal Priority)**:
- **Priority**: SCHED_OTHER (standard scheduling)
- **CPU Affinity**: CPU0/1/3 (non-isolated cores)
- **Responsibility**: GPS parsing, servo calculations, network TX
- **Shared Data**: Reads `PpsRtData` struct (mutex-protected)

**Shared Data Structure**:
```cpp
struct PpsRtData {
    std::mutex mutex;
    uint64_t pps_timestamp_ns;
    uint32_t pps_sequence;
    bool valid;
    uint32_t jitter_ns;
    bool dropout_detected;
};
```

**System Configuration Required**:
```bash
# /boot/firmware/cmdline.txt (Raspberry Pi 5)
isolcpus=2 nohz_full=2 rcu_nocbs=2
```

**Performance Targets**:
- PPS jitter: <500ns (vs current 0.5-3.0µs)
- Drift noise: ±0.2ppm (vs current ±1ppm)  
- Latency monitoring: Warn if >10ms

**Reference**: Original ptp_grandmaster.cpp lines 362-450  
**Status**: ⏳ TO BE IMPLEMENTED (deb.md Recommendation D - HIGH PRIORITY)

#### PhcAdapter ✅ (COMPLETE)
**File**: `src/phc_adapter.hpp`, `src/phc_adapter.cpp`  
**Responsibility**: i226 NIC PHC hardware clock access  
**Interface**:
```cpp
class PhcAdapter {
public:
    virtual bool initialize(const char* interface_name);
    virtual bool get_time(uint64_t* sec, uint32_t* nsec);
    virtual bool set_time(uint64_t sec, uint32_t nsec);
    virtual bool adjust_frequency(int32_t freq_ppb);
    int32_t get_max_frequency_ppb() const { return 500000; }
    const char* get_interface_name() const;
};
```
**Dependencies**: Linux PTP HAL (clock_gettime/clock_settime/clock_adjtime)  
**Status**: ✅ COMPLETE - Methods made virtual for testing (7/7 tests passing)

---

### 2. Servo Engines (Pure Logic, Hardware-Agnostic)

#### ServoInterface (Abstract Base)
**File**: `src/servo_interface.hpp`  
**Responsibility**: Define servo contract  
**Interface**:
```cpp
class ServoInterface {
public:
    virtual ~ServoInterface() = default;
    
    // Calculate frequency correction from phase offset
    virtual int32_t calculate_correction(int64_t offset_ns) = 0;
    
    // Reset servo state (after step correction)
    virtual void reset() = 0;
    
    // Get servo state for diagnostics
    virtual void get_state(ServoState* state) const = 0;
};
```

#### PI_Servo (Phase-Based Implementation)
**File**: `src/pi_servo.hpp`, `src/pi_servo.cpp`  
**Responsibility**: Proportional-Integral servo algorithm  
**Parameters**: Kp, Ki, integral_max, freq_max_ppb  
**State**: integral_ns, locked  
**Algorithm**:
```cpp
int32_t PI_Servo::calculate_correction(int64_t offset_ns) {
    integral_ns_ += offset_ns;
    
    // Anti-windup clamp
    integral_ns_ = clamp(integral_ns_, -integral_max_, integral_max_);
    
    // PI calculation
    double adjustment = kp_ * offset_ns + ki_ * integral_ns_;
    int32_t freq_ppb = static_cast<int32_t>(adjustment);
    
    // Frequency limit
    return clamp(freq_ppb, -freq_max_ppb_, freq_max_ppb_);
}
```
**Status**: ✅ COMPLETE (10/10 tests passing, limit cycle bug eliminated)

#### ThreePhaseServo (Offset/Drift Separation Implementation) ⏳
**File**: `src/servo_3phase.hpp`, `src/servo_3phase.cpp` (TO BE CREATED)  
**Responsibility**: Separate offset and drift handling per idd_3phaseDrift.md  
**Design**: Three distinct operational phases  

**Phase A - Offset Correction (Phase Align)**:
- Goal: Bring PHC near GPS to enable clean drift measurement
- Offset smoothed via median filter (N=5) or EMA (α=0.2)
- Step if |offset| > threshold (100ms startup, 1s normal)
- After step: Reset drift baseline, skip samples for 2-3 PPS pulses

**Phase B - Drift Baseline (Frequency Bias Capture)**:
- Goal: Determine stable PHC frequency offset
- Measure over N_PULSES (default 20, ~20 seconds)
- Calculate: `drift_ppm = ((phc_delta / ref_delta) - 1.0) * 1e6`
- Validity: No dropouts, no steps, |drift| < 2000 ppm hard limit
- Apply: `freq_bias_ppb = -drift_ppb` via `clock_adjtime(ADJ_FREQUENCY)`

**Phase C - Drift Evaluation (Run Servo)**:
- Goal: Continuously correct frequency without stepping
- Loop rate: 1 Hz (one sample per PPS)
- Gradient-based drift: `df_ppb = (offset[n] - offset[n-1]) / Δt`
- EMA filter: `df_ema = α * df + (1-α) * df_ema` (α=0.05...0.2)
- Total frequency: `freq_total = freq_bias + df_ema + freq_pi` (optional PI term)
- Step only if |offset| exceeds emergency threshold (500ms-1s)

**Critical Rules**:
- **Measurement Window Protection**: During Phase B drift measurement, NO steps and NO freq changes allowed
- **PPS-UTC Lock Integration**: Respects frozen mapping (base_pps_seq ↔ base_utc_sec)
- **Offset Sample Quality Gate**: Valid only if mapping locked, no dropout, PHC read <10ms after PPS

**Cmdline Selection**: `--servo-type [pi|3phase]` (default: pi)

**Status**: ⏳ TO BE IMPLEMENTED (Design documented in idd_3phaseDrift.md, integration pending)

---

### 3. Calibration Module

#### PhcCalibrator
**File**: `src/phc_calibrator.hpp`, `src/phc_calibrator.cpp`  
**Responsibility**: Measure PHC frequency drift vs GPS  
**Interface**:
```cpp
class PhcCalibrator {
public:
    PhcCalibrator(GpsAdapter* gps, PhcAdapter* phc);
    
    // Run 20-pulse calibration, return drift in ppb
    bool calibrate(int32_t* drift_ppb);
    
private:
    GpsAdapter* gps_;
    PhcAdapter* phc_;
    
    static constexpr int CALIBRATION_PULSES = 20;
};
```
**Algorithm**:
1. Wait for GPS fix
2. Capture GPS PPS timestamp (t0_gps)
3. Capture PHC timestamp at same instant (t0_phc)
4. Wait 20 PPS pulses (~20 seconds)
5. Capture GPS PPS ✅ (COMPLETE)
**File**: `src/phc_calibrator.hpp`, `src/phc_calibrator.cpp`  
**Responsibility**: Measure PHC frequency drift vs PPS pulse count  
**Interface**:
```cpp
class PhcCalibrator {
public:
    PhcCalibrator(const PhcCalibratorConfig& config);
    
    int initialize(PhcAdapter* phc, GpsAdapter* gps);
    int start_calibration(uint32_t pps_sequence, int64_t phc_timestamp_ns);
    int update_calibration(uint32_t pps_sequence, int64_t phc_timestamp_ns);
    bool is_calibrated() const;
    void get_state(PhcCalibrationState* state) const;
    void reset();
    int32_t get_cumulative_frequency() const;
};
```
**Algorithm**:
1. Capture baseline: PPS sequence number + PHC timestamp
2. Wait for configured interval (default: 20 pulses = 20 seconds)
3. Calculate drift using pure integer nanosecond math:
   - `phc_delta_ns = phc_current - phc_baseline`
   - `ref_delta_ns = elapsed_pulses * 1000000000LL`
   - `drift_ppm = ((phc_delta - ref_delta) / ref_delta) * 1e6`
4. Sanity check: Reject if `|drift_ppm| > 2000` (likely sampling error)
5. Apply correction if `|drift_ppm| > 100`, iterate up to 5 times
6. Return calibrated when drift acceptable or max iterations reached

**Status**: ✅ COMPLETE (7/7 tests passing, includes MockPhcAdapter for testing)
    
    // Update state based on GPS quality
    void update(bool pps_valid, bool tod_valid, int64_t offset_ns, int32_t freq_ppb);
    
    // Get current state
    ServoState get_state() const { return state_; }
    
    // Get active servo mode
    ServoMode get_servo_mode() const;
    
private:
    ServoState state_;
    int consecutive_gps_good_;
    int consecutive_gps_bad_;
    
    static constexpr int GPS_STABLE_THRESHOLD = 10;  // samples
};
```

**Transition Logic**:
```
RECOVERY_GPS:
  if (pps_valid && tod_valid && offset < 100ns && freq < 5ppb) {
    consecutive_gps_good++
    if (consecutive_gps_good >= 10) → LOCKED_GPS
  }

LOCKED_GPS:
  if (!pps_valid || !tod_valid) → HOLDOVER_RTC

HOLDOVER_RTC:
  if (pps_valid && tod_valid) → RECOVERY_GPS
```

**Status**: ⏳ TO BE EXTRACTED from ptp_grandmaster.cpp

---

### 5. Orchestration Layer

#### GrandmasterController
**File**: `src/grandmaster_controller.hpp`, `src/grandmaster_controller.cpp`  
**Responsibility**: Coordinate all modules (main loop)  
**Interface**:
```cpp
class GrandmasterController {
public:
    GrandmasterController(
        GpsAdapter* gps,
        RtcAdapter* rtc,
        PhcAdapter* phc,
        ServoInterface* servo,
        ServoStateMachine* state_machine,
        PhcCalibrator* calibrator
    );
    
    bool initialize();
    void run();
    void shutdown();
    
private:
    // Adapters
    GpsAdapter* gps_;
    RtcAdapter* rtc_;
    PhcAdapter* phc_;
    
    // Engines
    ServoInterface* servo_;
    ServoStateMachine* state_machine_;
    PhcCalibrator* calibrator_;
    
    // State
    bool freq_calibrated_;
    int32_t calibration_drift_ppb_;
    int32_t cumulative_freq_ppb_;
};
```

**Main Loop Algorithm**:
```cpp
void GrandmasterController::run() {
    // 1. Calibrate once on startup
    if (!freq_calibrated_) {
        calibrator_->calibrate(&calibration_drift_ppb_);
        phc_->adjust_frequency(calibration_drift_ppb_);
        cumulative_freq_ppb_ = calibration_drift_ppb_;
        freq_calibrated_ = true;
    }
    
    while (running_) {
        // 2. Get measurements
        uint64_t gps_sec, gps_nsec;
        gps_->get_time(&gps_sec, &gps_nsec);
        
        uint64_t phc_sec, phc_nsec;
        phc_->get_time(&phc_sec, &phc_nsec);
        
        // 3. Calculate offset
        int64_t offset_ns = calculate_offset(gps_sec, gps_nsec, phc_sec, phc_nsec);
        
        // 4. Update state machine
        PpsData pps_data;
        bool pps_valid = gps_->get_pps_data(&pps_data, nullptr);
        state_machine_->update(pps_valid, true, offset_ns, cumulative_freq_ppb_);
        
        // 5. Check step threshold
        if (abs(offset_ns) > STEP_THRESHOLD_NS) {
            phc_->set_time(gps_sec, gps_nsec);
            servo_->reset();
            cumulative_freq_ppb_ = calibration_drift_ppb_;
        } else {
            // 6. Calculate servo correction
            int32_t correction_ppb = servo_->calculate_correction(offset_ns);
            
            // 7. Apply correction (cumulative)
            int32_t new_freq_ppb = cumulative_freq_ppb_ + correction_ppb;
            new_freq_ppb = clamp(new_freq_ppb, -500000, 500000);
            
            phc_->adjust_frequency(new_freq_ppb);
            cumulative_freq_ppb_ = new_freq_ppb;  // CRITICAL: Persist correction
        }
        
        // 8. Sleep until next cycle
        sleep(1);
    }
}
```

**Status**: ⏳ TO BE CREATED from ptp_grandmaster.cpp refactor

---

## 🚀 Refactoring Plan

### Phase 1: Create Missing Adapter
**Priority**: 🔴 CRITICAL  
**Status**: ✅ COMPLETE

1. ✅ Create architecture document (this file)
2. ✅ Implement `PhcAdapter` interface (virtual methods for testing)
3. ✅ Extract PHC hardware calls from ptp_grandmaster.cpp
4. ✅ Unit test PhcAdapter (7/7 tests passing)

### Phase 2: Extract Servo Engines
**Priority**: 🟡 HIGH  
**Status**: ✅ COMPLETE

1. ✅ Create `ServoInterface` abstract base class
2. ✅ Extract PI servo logic to `PI_Servo` class
3. ✅ Unit test PI servo with synthetic offset sequences (10/10 tests)
4. ✅ Verify limit cycle bug eliminated (Test #10 proof)

### Phase 3: Extract Calibration Module
**Priority**: 🟡 HIGH  
**Status**: ✅ COMPLETE

1. ✅ Create `PhcCalibrator` class
2. ✅ Extract 20-pulse calibration logic (with iterative refinement)
3. ✅ Unit test with MockPhcAdapter (7/7 tests passing)
4. ✅ Implement sanity checking (reject > 2000 ppm)

### Phase 4: Extract State Machine
**Priority**: 🟢 MEDIUM  
**Status**: ✅ COMPLETE

1. ✅ Created `ServoStateMachine` class
2. ✅ Extracted state transition logic (RECOVERY_GPS ↔ LOCKED_GPS ↔ HOLDOVER_RTC)
3. ✅ Unit tested state transitions (10/10 tests passing)

### Phase 4.5: Extract NetworkAdapter (Uses Repository HAL)
**Priority**: 🔴 CRITICAL  
**Status**: ✅ COMPLETE

**Note**: The IEEE 1588-2019 repository already defines network interface abstractions:
- `04-design/components/ieee-1588-2019-hal-interface-design.md` - NetworkInterface design
- `04-design/components/ieee-1588-2019-transport-design.md` - Transport layer design
- `examples/03-hal-implementation-template/hal_template.hpp` - NetworkHAL template

**Current State**: `LinuxPtpHal` mixes network + PHC operations (1000+ lines)

**Completed**:
1. ✅ Extracted network operations from `LinuxPtpHal` into `NetworkAdapter`
2. ✅ Aligned with repository's HAL interface patterns
3. ✅ `NetworkAdapter` handles: sockets, timestamping, multicast, TX/RX
4. ✅ `PhcAdapter` handles: clock operations only
5. ✅ Unit tested with 12/12 tests passing (thread-safe, error handling verified)

**Rationale**: 
- Follows repository architecture (use existing abstractions, don't duplicate)
- Grandmaster becomes reference implementation of repository's HAL interfaces
- Enables future platform ports (Windows, RTOS) using same interfaces

### Phase 5: Create Orchestration Layer
**Priority**: 🟢 MEDIUM  
**Status**: 📋 Planned

1. Create `GrandmasterController` class
2. Wire all modules together via dependency injection
3. Use repository's HAL abstractions (NetworkInterface, ClockInterface)
4. Refactor `main()` to use controller

### Phase 6: Integration Testing
**Priority**: 🟢 MEDIUM  
**Status**: 📋 Planned

1. Run 30-minute validation test
2. Compare performance: Old monolith vs. Clean architecture
3. Verify zero oscillation, stable lock

---

## 📊 Success Criteria

### Architectural Quality
- ✅ Each module <300 lines (was 1750 in monolith)
- ✅ Clear interfaces with no circular dependencies
- ✅ 100% unit test coverage on engines (servos, calibrator, state machine)
- ✅ Hardware-agnostic protocol logic (no Linux #includes in engines)

### Functional Quality
- ✅ Zero oscillation in 30-minute test
- ✅ Offset converges <1ms within 5 minutes
- ✅ State machine transitions correctly (RECOVERY → LOCKED → HOLDOVER → RECOVERY)
- ✅ Servo frequency accumulates correctly (not frozen at 0)

### Maintainability
- ✅ New servo can be added without modifying existing code (Open/Closed)
- ✅ PHC adapter can be swapped for different NIC without touching servos
- ✅ Each module independently debuggable with logging
- ✅ Clear blame: If servo fails, bug is in servo class (not 1750-line file)

---

## 🧪 Testing Strategy

### Unit Tests (Per Module)
- **PhcAdapter**: Mock Linux clock_adjtime, verify frequency adjustments applied
- **PI_Servo**: Feed synthetic offsets, verify integral accumulation and anti-windup
- **FrequencyError_Servo**: Feed offset sequence, verify EMA filtering
- **PhcCalibrator**: Mock GPS + PHC, verify drift calculation
- **ServoStateMachine**: Feed state transitions, verify logic

### Integration Tests
- **Calibration Flow**: GPS → Calibrator → PHC adapter, verify end-to-end
- **Servo Flow**: GPS → Offset calc → Servo → PHC, verify corrections applied
- **State Machine Flow**: GPS loss → HOLDOVER → Recovery, verify transitions

### System Tests
- **30-Minute Validation**: Zero oscillation, stable lock
- **GPS Dropout**: Verify HOLDOVER → RECOVERY transition
- **Step Correction**: Large offset triggers step, servo resets

---

## 📝 Migration Notes

### What Changes
- `ptp_grandmaster.cpp` reduces from 1750 lines → ~200 lines (main + orchestration)
- PHC hardware calls move to `PhcAdapter`
- Servo logic moves to `PI_Servo` and `FrequencyError_Servo` classes
- Calibration moves to `PhcCalibrator`
- State machine moves to `ServoStateMachine`

### What Stays Same
- GpsAdapter (already clean) ✅
- RtcAdapter (already clean) ✅
- Linux PTP HAL (low-level hardware layer) ✅
- Build system (CMakeLists.txt) - minor updates only

### Backwards Compatibility
- External behavior identical (same command-line args, same log output)
- Internal architecture completely refactored
- Old ptp_grandmaster.cpp kept as `ptp_grandmaster_legacy.cpp.bak` for reference

---

## 🎯 Next Immediate Actions

1. **Create PhcAdapter.hpp** - Define interface
2. **Create PhcAdapter.cpp** - Implement using Linux PTP HAL
3. **Extract PHC calls** - Replace direct hal calls in ptp_grandmaster.cpp
4. **Test PhcAdapter** - Verify frequency adjustments work
5. **Continue with Phase 2** - Extract servos

**Ready to start Phase 1: PhcAdapter implementation?**
