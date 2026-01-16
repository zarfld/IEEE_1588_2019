---
title: "DriftObserver Data Structures Design"
version: "1.0"
date: "2025-01-16"
status: "Draft"
phase: "04-design"
traces_to: 
  - "drift_observer_spec.md"
  - "drift_observer_requirements.md"
verified_by: []
---

# DriftObserver Data Structures Design

## Purpose
This document defines the data structures for the DriftObserver class based on the specification in `docs/drift_observer_spec.md`. These structures implement a moving-window drift observation system with epoch tracking, contamination event handling, and robust statistical analysis.

**Specification Reference**: `examples/12-RasPi5_i226-grandmaster/docs/drift_observer_spec.md`  
**Requirements**: `examples/12-RasPi5_i226-grandmaster/docs/drift_observer_requirements.md`

## Design Philosophy
- **Separation of Concerns**: Offset (phase error) vs Drift (frequency error)
- **Epoch Tracking**: Samples valid only within same epoch (contamination isolation)
- **Quality Gating**: Expose ready/trustworthy/health flags for servo decision-making
- **Event-Driven**: Explicit notifications for clock steps, frequency adjustments, reference changes
- **Testability**: All structures designed for unit testing with deterministic behavior

---

## 1. DriftSample Structure

**Purpose**: Store raw and derived measurements for each PPS tick

**Specification Reference**: Section 3 "Moving window ring buffer: what to store per tick"

```cpp
/**
 * @brief Single drift observation sample (one PPS tick)
 * 
 * Stores both raw timestamps and computed deltas to enable:
 * - Offset calculation: t_clk - t_ref
 * - Drift calculation: (offset[k] - offset[k-1]) / dt
 * - Quality checks: dt_ref should be ~1e9 ns (1 second)
 * - Outlier detection: flag spikes in offset or drift
 * 
 * Specification: Section 3, drift_observer_spec.md
 */
struct DriftSample {
    // Sample metadata
    uint64_t seq;              ///< Monotonic sample index (0, 1, 2, ...)
    uint64_t epoch_id;         ///< Epoch identifier (increments on contamination events)
    
    // Raw timestamps (nanoseconds)
    int64_t t_ref_ns;          ///< Reference timestamp at PPS tick (GPS time)
    int64_t t_clk_ns;          ///< Measured clock timestamp at PPS tick
    
    // Computed offset and deltas
    int64_t offset_ns;         ///< Phase error: t_clk - t_ref
    int64_t dt_ref_ns;         ///< Time delta on reference: t_ref[k] - t_ref[k-1] (ideally 1e9)
    int64_t dt_clk_ns;         ///< Time delta on clock: t_clk[k] - t_clk[k-1]
    
    // Computed drift (frequency error)
    int64_t drift_ns_per_s;    ///< Instantaneous drift: offset[k] - offset[k-1]
    
    // Quality flags
    bool valid;                ///< Sample passed quality checks
    uint32_t flags;            ///< Bitmask: spike/outlier/jitter indicators
    
    // Sample flag bits
    static constexpr uint32_t FLAG_OFFSET_SPIKE    = 1 << 0;  ///< Offset jump detected
    static constexpr uint32_t FLAG_DRIFT_SPIKE     = 1 << 1;  ///< Drift outlier detected
    static constexpr uint32_t FLAG_DT_REF_INVALID  = 1 << 2;  ///< dt_ref deviated from 1s
    static constexpr uint32_t FLAG_DT_CLK_INVALID  = 1 << 3;  ///< dt_clk suspicious
    static constexpr uint32_t FLAG_EPOCH_BOUNDARY  = 1 << 4;  ///< First sample after epoch change
    static constexpr uint32_t FLAG_IN_HOLDOFF      = 1 << 5;  ///< Sample during holdoff period
};
```

**Design Rationale**:
- `seq`: Enables debugging and gap detection
- `epoch_id`: Ensures drift only computed within same epoch (REQ-6.1, REQ-6.2)
- `dt_ref_ns`, `dt_clk_ns`: Detect missed ticks, bad reference (REQ-2.6, REQ-5.1)
- `drift_ns_per_s`: Pre-computed for efficiency (avoid recomputation in statistics)
- `flags`: Detailed diagnostics for outlier rejection and debugging

---

## 2. Config Structure

**Purpose**: Configuration parameters for DriftObserver

**Specification Reference**: Section 7 "Core API" - Config struct

```cpp
/**
 * @brief Configuration for DriftObserver behavior
 * 
 * Controls window size, outlier rejection, drift estimation method,
 * and quality thresholds.
 * 
 * Specification: Section 7, drift_observer_spec.md
 * Recommended defaults: Section 11, drift_observer_spec.md
 */
struct Config {
    // Ring buffer configuration
    size_t window_size;                  ///< Number of samples in window (e.g., 120)
    size_t min_valid_samples;            ///< Minimum samples before ready (e.g., 30)
    
    // Quality thresholds
    int64_t max_dt_ref_deviation_ns;     ///< Max deviation of dt_ref from 1e9 (e.g., 2ms)
    int64_t max_offset_step_ns;          ///< Offset step threshold (e.g., 1ms)
    int64_t max_drift_ppm;               ///< Maximum plausible drift (e.g., 500 ppm)
    
    // Outlier detection
    double outlier_mad_sigma;            ///< MAD-based outlier threshold (e.g., 4.5)
    double max_invalid_ratio;            ///< Max fraction of invalid samples (e.g., 0.10)
    
    // Drift estimation method
    bool use_linear_regression;          ///< true: fit slope, false: mean of deltas
    
    // Holdoff/settle timing
    uint32_t holdoff_after_step_ticks;   ///< Ticks to wait after clock step (e.g., 5)
    uint32_t holdoff_after_freq_ticks;   ///< Ticks to wait after frequency adjust (e.g., 2)
    uint32_t holdoff_after_ref_ticks;    ///< Ticks to wait after reference change (e.g., 10)
    
    // Trust gating
    double max_drift_stddev_ppm;         ///< Max drift stddev for "stable" (e.g., 5.0)
    
    /**
     * @brief Create recommended default configuration
     * Specification: Section 11 "Recommended first version configuration"
     */
    static Config CreateDefault() {
        return Config{
            .window_size = 120,                    // 2 minutes at 1 Hz
            .min_valid_samples = 30,               // 30 seconds warmup
            .max_dt_ref_deviation_ns = 2'000'000,  // 2 ms
            .max_offset_step_ns = 1'000'000,       // 1 ms
            .max_drift_ppm = 500,                  // 500 ppm max plausible
            .outlier_mad_sigma = 4.5,              // MAD threshold
            .max_invalid_ratio = 0.10,             // 10% invalid ok
            .use_linear_regression = true,         // Recommended
            .holdoff_after_step_ticks = 5,         // 5 seconds
            .holdoff_after_freq_ticks = 2,         // 2 seconds
            .holdoff_after_ref_ticks = 10,         // 10 seconds
            .max_drift_stddev_ppm = 5.0            // 5 ppm stddev
        };
    }
};
```

**Design Rationale**:
- `window_size`: Trade-off between noise reduction and responsiveness (REQ-3.1)
- `min_valid_samples`: Prevent premature trust (REQ-7.1)
- Thresholds: Environment-specific, defaults from Section 11 of spec
- `use_linear_regression`: Best practice per Section 5 Method B (REQ-4.2)
- Holdoff ticks: Contamination event settling (REQ-7.2, Section 4 "Holdoff timer")

---

## 3. Estimate Structure

**Purpose**: Output of DriftObserver - robust statistics and quality flags

**Specification Reference**: Section 7 "Core API" - Estimate struct

```cpp
/**
 * @brief Drift observation estimate with quality flags
 * 
 * Provides robust statistics on offset and drift, along with
 * readiness/trustworthiness indicators for servo decision-making.
 * 
 * Specification: Section 7, drift_observer_spec.md
 * Trust gating: Section 8 "Define trustworthiness with quantitative gate"
 */
struct Estimate {
    // Quality gates
    bool ready;                ///< Enough valid samples for computation
    bool trustworthy;          ///< Clean history, out of holdoff, low jitter
    
    // Offset statistics (phase error)
    int64_t offset_mean_ns;    ///< Mean offset over window
    int64_t offset_stddev_ns;  ///< Standard deviation of offset
    int64_t offset_median_ns;  ///< Median offset (for MAD calculation)
    
    // Drift statistics (frequency error)
    double drift_ppm;          ///< Estimated drift in parts per million
    double drift_stddev_ppm;   ///< Standard deviation of drift
    
    // Jitter and health
    double jitter_ns_rms;      ///< RMS jitter (optional quality metric)
    uint32_t health_flags;     ///< Bitmask: HealthFlags enum
    
    // Sample counts
    size_t total_samples;      ///< Total samples in window
    size_t valid_samples;      ///< Valid samples in window
    
    // Epoch tracking
    uint64_t current_epoch;    ///< Current epoch ID
    uint64_t ticks_in_epoch;   ///< Ticks since epoch started
    uint64_t ticks_in_holdoff; ///< Remaining holdoff ticks (0 if not in holdoff)
    
    /**
     * @brief Check if estimate is usable for offset correction
     * @return true if ready and reference is good
     */
    bool CanCorrectOffset() const {
        return ready && !(health_flags & (HF_REFERENCE_BAD | HF_NOT_READY));
    }
    
    /**
     * @brief Check if estimate is usable for drift/frequency correction
     * @return true if trustworthy and not in holdoff
     */
    bool CanCorrectDrift() const {
        return trustworthy && !(health_flags & (HF_IN_HOLDOFF | HF_STEP_DETECTED));
    }
};
```

**Design Rationale**:
- `ready` vs `trustworthy`: Two-level quality gate (REQ-7.1, REQ-7.3)
- `offset_median_ns`: Required for MAD-based outlier detection (REQ-5.2)
- `drift_ppm`: Primary servo input (REQ-4.1)
- `health_flags`: Explicit reasons for non-trust (Section 9 "Expose structured health output")
- Helper methods: Servo decision logic (Section 8 "Use Policy B - Reset window")

---

## 4. HealthFlags Enumeration

**Purpose**: Bitmask flags indicating why estimate is not trustworthy

**Specification Reference**: Section 9 "Expose a structured health output to the servo"

```cpp
/**
 * @brief Health status flags for DriftObserver estimate
 * 
 * Bitmask flags indicating various quality/trust issues.
 * Multiple flags can be set simultaneously.
 * 
 * Specification: Section 9, drift_observer_spec.md
 */
enum HealthFlags : uint32_t {
    HF_NONE                  = 0,        ///< No issues, estimate is healthy
    
    // Readiness issues
    HF_NOT_READY             = 1 << 0,   ///< Not enough samples yet
    HF_IN_HOLDOFF            = 1 << 1,   ///< In settle period after event
    
    // Reference quality issues
    HF_REFERENCE_BAD         = 1 << 2,   ///< PPS source unreliable/lost
    HF_MISSING_TICKS         = 1 << 3,   ///< Gaps in tick sequence detected
    
    // Contamination issues
    HF_STEP_DETECTED         = 1 << 4,   ///< Clock step detected (epoch changed)
    HF_WINDOW_CONTAMINATED   = 1 << 5,   ///< Too many invalid samples in window
    
    // Stability issues
    HF_JITTER_TOO_HIGH       = 1 << 6,   ///< drift_stddev exceeds threshold
    HF_OFFSET_UNSTABLE       = 1 << 7,   ///< offset_stddev too large
};
```

**Design Rationale**:
- Bitmask design: Multiple issues can coexist (REQ-8.1)
- Flags map to servo policy decisions (Section 9 "Servo policy can be deterministic")
- Categories: Readiness, Reference quality, Contamination, Stability (REQ-12.1)

---

## 5. ObserverEvent Enumeration

**Purpose**: Explicit notifications for contamination events

**Specification Reference**: Section 6 "Put this into class design: event API"

```cpp
/**
 * @brief Events that contaminate drift observation history
 * 
 * These events require epoch transitions, window resets, or holdoff periods.
 * The DriftObserver must be explicitly notified via NotifyEvent() to maintain
 * correctness.
 * 
 * Specification: Section 6, drift_observer_spec.md
 * Contamination events: Section 5 (A-E)
 */
enum class ObserverEvent {
    // Reference changes (Section 5.A)
    ReferenceChanged,       ///< PPS source changed (GPS -> RTC, etc.)
    ReferenceLost,          ///< PPS missing/unreliable
    ReferenceRecovered,     ///< PPS restored after loss
    
    // Clock discontinuities (Section 5.B)
    ClockStepped,           ///< clock_settime() or big offset correction
    ClockSlewed,            ///< Small smooth correction (optional)
    
    // Frequency changes (Section 5.C)
    FrequencyAdjusted,      ///< adjfreq() or PI controller tuning change
    
    // Servo state changes (Section 5.C)
    ServoModeChanged,       ///< Free-run -> disciplined, or vice versa
    
    // Manual triggers
    WarmStartRequested,     ///< Operator requests reset and reacquire
};
```

**Design Rationale**:
- Each event maps to specific contamination scenario (Section 5)
- Enables deterministic epoch management (REQ-6.3)
- "Belt and suspenders": Automatic detection + explicit notification (Section 7)

---

## 6. DriftObserver Class API

**Purpose**: Main observer class with Update and GetEstimate methods

**Specification Reference**: Section 7 "Proposed class API"

```cpp
/**
 * @brief Generic drift observer with moving window and epoch tracking
 * 
 * Observes offset and drift between two clocks (e.g., GPS-RTC, GPS-PHC).
 * - Triggered every PPS tick for second-by-second measurements
 * - Maintains ring buffer with configurable window size
 * - Detects and filters outliers using MAD-based spike detection
 * - Tracks epochs to isolate contamination events
 * - Provides robust offset/drift estimates with quality gates
 * 
 * Usage:
 *   Config cfg = Config::CreateDefault();
 *   DriftObserver obs(cfg, "GPS-RTC");
 *   
 *   // Every PPS tick:
 *   obs.Update(gps_time_ns, rtc_time_ns);
 *   
 *   // Query estimate:
 *   Estimate est = obs.GetEstimate();
 *   if (est.CanCorrectDrift()) {
 *       servo.SetFrequency(est.drift_ppm);
 *   }
 * 
 * Specification: Section 7, drift_observer_spec.md
 */
class DriftObserver {
public:
    /**
     * @brief Construct drift observer
     * @param config Configuration parameters
     * @param name Human-readable name (e.g., "GPS-RTC", "GPS-PHC")
     */
    DriftObserver(const Config& config, const std::string& name);
    
    /**
     * @brief Update observer with new PPS tick sample
     * 
     * Called exactly once per PPS tick. Computes offset, drift, performs
     * outlier detection, and updates ring buffer.
     * 
     * @param t_ref_ns Reference timestamp at PPS (e.g., GPS TAI time)
     * @param t_clk_ns Measured clock timestamp at PPS
     * 
     * Specification: Section 2 "Define what you want to observe"
     * Requirements: REQ-2.1, REQ-2.2, REQ-2.3
     */
    void Update(int64_t t_ref_ns, int64_t t_clk_ns);
    
    /**
     * @brief Get current drift estimate
     * 
     * Returns robust statistics over moving window with quality flags.
     * Servo should check ready/trustworthy flags before using.
     * 
     * @return Estimate structure with offset/drift stats and health flags
     * 
     * Specification: Section 7 "GetEstimate()"
     * Requirements: REQ-8.2, REQ-8.3
     */
    Estimate GetEstimate() const;
    
    /**
     * @brief Get most recent sample
     * 
     * Provides access to latest raw sample for debugging and diagnostics.
     * 
     * @return Reference to most recent DriftSample
     * 
     * Specification: Section 7 "Latest()"
     * Requirements: REQ-12.2
     */
    const DriftSample& Latest() const;
    
    /**
     * @brief Notify observer of contamination event
     * 
     * Explicit notification for events that invalidate drift history.
     * Triggers epoch increment, window reset, or holdoff as appropriate.
     * 
     * @param event Event type
     * @param magnitude_ns Optional magnitude (for steps/slews)
     * 
     * Specification: Section 6 "event API"
     * Requirements: REQ-6.3, REQ-7.2
     */
    void NotifyEvent(ObserverEvent event, int64_t magnitude_ns = 0);
    
    /**
     * @brief Reset observer (clear all history)
     * 
     * Clears ring buffer, increments epoch, resets all state.
     * Use for operator-requested reset or catastrophic failures.
     * 
     * Requirements: REQ-6.4
     */
    void Reset();
    
private:
    // Configuration
    Config config_;
    std::string name_;
    
    // Ring buffer
    std::vector<DriftSample> samples_;
    size_t write_index_;
    size_t sample_count_;
    
    // Epoch tracking
    uint64_t current_epoch_;
    uint64_t sample_seq_;
    
    // Holdoff state
    uint32_t holdoff_ticks_remaining_;
    
    // Previous sample (for delta computation)
    int64_t prev_t_ref_ns_;
    int64_t prev_t_clk_ns_;
    int64_t prev_offset_ns_;
    bool first_sample_;
    
    // Cached statistics (dirty flag pattern)
    mutable bool stats_dirty_;
    mutable Estimate cached_estimate_;
    
    // Internal methods
    void ComputeStatistics() const;
    void DetectOutliers(DriftSample& sample);
    bool IsOffsetSpike(int64_t offset_ns) const;
    bool IsDriftSpike(int64_t drift_ns_per_s) const;
    void IncrementEpoch();
    void ClearWindow();
    double ComputeDriftLinearRegression() const;
    double ComputeDriftMean() const;
    double ComputeMAD(const std::vector<int64_t>& values) const;
};
```

**Design Rationale**:
- `Update()`: Core per-tick processing (REQ-2.1, REQ-2.2, REQ-2.3)
- `GetEstimate()`: Servo interface with quality gates (REQ-8.2, REQ-8.3)
- `NotifyEvent()`: Explicit contamination handling (REQ-6.3, Section 6)
- Private methods: Encapsulate complexity (linear regression, MAD, outlier detection)
- Dirty flag pattern: Avoid recomputing statistics every call (performance)

---

## 7. Design Decisions and Rationale

### 7.1 Epoch-Based Contamination Isolation
**Decision**: Use `epoch_id` per sample, increment on contamination events  
**Rationale**: Per Section 2 "Epochs and contamination events", drift is trustworthy only within same epoch  
**Alternative Rejected**: Age-out policy (Section 3 Policy A) - too slow to recover after step

### 7.2 Policy B - Reset Window on Contamination
**Decision**: Clear ring buffer on clock steps, reference changes  
**Rationale**: Section 3 Policy B - "deterministic and clean"  
**Tradeoff**: Lose history but gain rapid recovery (30s warmup vs 120s age-out)

### 7.3 MAD-Based Outlier Detection
**Decision**: Use Median Absolute Deviation with configurable sigma threshold  
**Rationale**: Section 5 Method A "MAD (median absolute deviation)", more robust than mean/stddev for fat-tailed distributions  
**Default**: 4.5 sigma (Section 11 recommended config)

### 7.4 Linear Regression as Primary Drift Estimator
**Decision**: Fit slope of offset vs time (Method B)  
**Rationale**: Section 5 Method B - "superior because it uses all samples and suppresses noise"  
**Fallback**: Mean of drift deltas if `use_linear_regression = false`

### 7.5 Two-Level Quality Gates
**Decision**: `ready` (enough samples) vs `trustworthy` (clean + stable)  
**Rationale**: Section 8 "quantitative gate" - servo needs to know WHY estimate is not usable  
**Implementation**: `ready` checks sample count, `trustworthy` checks holdoff, jitter, contamination

### 7.6 Holdoff Timers per Event Type
**Decision**: Separate holdoff durations for step/freq/ref events  
**Rationale**: Section 4 "Holdoff / Settle timer" - different events have different settling times  
**Defaults**: Step=5s, Freq=2s, Ref=10s (from Section 11)

### 7.7 Automatic Step Detection
**Decision**: Detect steps from data even with NotifyEvent API  
**Rationale**: Section 7 "belt and suspenders" - catches unexpected steps from other daemons  
**Threshold**: `max_offset_step_ns` (default 1ms per Section 11)

---

## 8. Implementation Notes

### 8.1 Ring Buffer Mechanics
- Use `write_index_` modulo `window_size` for circular buffer
- Track `sample_count_` separately (0 to `window_size`)
- Oldest sample overwritten when full (FIFO behavior)

### 8.2 Drift Calculation Options
```cpp
// Method A: Mean of drift deltas
double drift_mean = sum(drift_ns_per_s) / count;

// Method B: Linear regression (recommended)
// Fit offset = a + b*t, where b is drift
double drift_slope = least_squares_slope(offset_ns, time_index);
```

### 8.3 MAD Calculation
```cpp
// Median Absolute Deviation
double median_offset = compute_median(offset_ns);
vector<double> abs_deviations;
for (auto off : offset_ns) {
    abs_deviations.push_back(abs(off - median_offset));
}
double MAD = compute_median(abs_deviations);

// Outlier threshold: median ± (MAD * sigma)
double threshold = outlier_mad_sigma * MAD * 1.4826;  // 1.4826 = scaling factor
```

### 8.4 Statistics Update Optimization
- Set `stats_dirty_ = true` on every `Update()`
- Recompute only when `GetEstimate()` called
- Cache result to avoid redundant computation

### 8.5 Thread Safety Considerations
**Current Design**: Single-threaded (called from PPS interrupt context)  
**Future**: If multi-threaded, add mutex around `Update()` and `GetEstimate()`

---

## 9. Testing Strategy (Phase 3 Reference)

These data structures enable the following test phases:

**Phase 1: Data Structure Tests**
- Verify `DriftSample` field population
- Verify `Config::CreateDefault()` values
- Verify `Estimate` flag combinations
- Verify `HealthFlags` bitmask operations

**Phase 2: Ring Buffer Tests**
- Verify circular buffer wraparound
- Verify sample_count vs window_size
- Verify epoch filtering (only same epoch in statistics)

**Phase 3: Spike Detection Tests**
- Verify MAD-based outlier rejection
- Verify offset step detection
- Verify drift spike detection
- Verify dt_ref validation

**Phase 4: Drift Estimation Tests**
- Verify linear regression slope calculation
- Verify mean of deltas fallback
- Verify convergence with stable input
- Verify response to systematic drift

**Phase 5: Epoch/Contamination Tests**
- Verify epoch increment on NotifyEvent()
- Verify window reset on ClockStepped
- Verify cross-epoch samples rejected from stats

**Phase 6: Holdoff/Trust Tests**
- Verify holdoff timer behavior
- Verify ready vs trustworthy transitions
- Verify HealthFlags set correctly

**Phase 7: Event Handling Tests**
- Verify all ObserverEvent types
- Verify automatic step detection
- Verify recovery after reference loss

**Phase 8: Integration Tests**
- Multi-clock observers (GPS-RTC, GPS-PHC, RTC-SYS)
- Realistic GPS PPS timing
- Noise injection and outlier patterns

---

## 10. Traceability Matrix

| Data Structure | Specification Section | Requirements |
|---|---|---|
| DriftSample | Section 3 | REQ-2.1, REQ-2.2, REQ-2.3, REQ-2.4, REQ-2.5 |
| Config | Section 7, 11 | REQ-3.1, REQ-12.1 |
| Estimate | Section 7, 8 | REQ-8.1, REQ-8.2, REQ-8.3 |
| HealthFlags | Section 9 | REQ-8.1, REQ-12.1 |
| ObserverEvent | Section 6 | REQ-6.3, REQ-7.2 |
| DriftObserver | Section 7 | REQ-1.1 through REQ-12.3 |

---

## 11. Next Steps

**Immediate** (Step 3 - RED Phase):
1. Create test file: `tests/test_drift_observer_structures.cpp`
2. Write tests for each data structure (Phase 1 of test plan)
3. Write tests for ring buffer mechanics (Phase 2)
4. Run tests → ALL FAIL (RED phase)

**After RED** (Step 4 - GREEN Phase):
1. Implement DriftObserver class in `src/drift_observer.cpp`
2. Implement minimal code to pass each test
3. Run tests → ALL PASS (GREEN phase)

**After GREEN** (Step 5 - REFACTOR):
1. Optimize statistics calculation
2. Add instrumentation/logging
3. Integration into grandmaster_controller.cpp

---

## 12. Open Questions for Review

1. **Thread Safety**: Should DriftObserver support concurrent access? (Current: single-threaded assumption)
2. **Config Validation**: Should Config constructor validate ranges (e.g., window_size > 0)?
3. **Estimate Lifetime**: Should Estimate struct be copyable or return by const reference?
4. **Event Magnitude**: How to use `magnitude_ns` parameter in NotifyEvent()? (Step detection threshold override?)

---

## References

- **Specification**: `examples/12-RasPi5_i226-grandmaster/docs/drift_observer_spec.md`
- **Requirements**: `examples/12-RasPi5_i226-grandmaster/docs/drift_observer_requirements.md`
- **IEEE 1588-2019**: Section 7.4 (Clock model and characteristics)
- **Related**: ISO/IEC/IEEE 42010:2011 (Architecture description)

---

**Status**: Ready for Step 3 (RED phase testing)  
**Author**: AI Assistant (GitHub Copilot)  
**Review**: Pending user approval
