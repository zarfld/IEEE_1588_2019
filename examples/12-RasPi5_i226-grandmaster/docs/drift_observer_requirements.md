# DriftObserver Requirements Checklist
**Source**: drift_observer_spec.md  
**Date**: 2026-01-16  
**TDD Phase**: Requirements Analysis (Step 1 of 4)

## 1. Core Concept Requirements

### REQ-1.1: Tick Source Consistency
- [ ] Observer uses **one PPS source** consistently (GPS PPS or RTC SQW)
- [ ] All clock measurements happen **at the same PPS tick**
- [ ] Provides common timebase for statistics
- **Test**: Verify single tick source throughout operation

### REQ-1.2: Offset vs Drift Separation
- [ ] **Offset** = instantaneous phase error = `t_clk[k] - t_ref[k]`
- [ ] **Drift** = frequency error = rate of change of offset
- [ ] Drift formula: `drift[k] ≈ offset[k] - offset[k-1]`
- [ ] Drift units: ppm = `drift[k] × 10^6`
- **Test**: Verify offset and drift calculations are independent

## 2. Data Structure Requirements

### REQ-2.1: DriftSample Structure
Must store per-tick:
- [ ] `uint64_t seq` - monotonic sample index
- [ ] `int64_t t_ref_ns` - reference timestamp at tick (ns)
- [ ] `int64_t t_clk_ns` - measured clock timestamp at tick (ns)
- [ ] `int64_t offset_ns` - computed as `t_clk - t_ref`
- [ ] `int64_t dt_ref_ns` - `t_ref[k] - t_ref[k-1]` (ideally 1e9)
- [ ] `int64_t dt_clk_ns` - `t_clk[k] - t_clk[k-1]`
- [ ] `int64_t drift_ns_per_s` - `offset[k] - offset[k-1]`
- [ ] `bool valid` - sample validity flag
- [ ] `uint32_t flags` - spike/outlier/jitter indicators
- **Test**: Verify all fields populated correctly

### REQ-2.2: Config Structure
Must configure:
- [ ] `size_t window_size` - ringbuffer size (e.g., 120 samples)
- [ ] `int64_t max_dt_ref_deviation_ns` - tick timing tolerance (e.g., 2ms)
- [ ] `int64_t max_offset_step_ns` - offset jump threshold (e.g., 1ms)
- [ ] `double outlier_mad_sigma` - MAD-based spike detection (e.g., 4.5)
- [ ] `size_t min_valid_samples` - minimum for trustworthy estimate (e.g., 30)
- [ ] `bool use_linear_regression` - use Method B vs Method A
- **Test**: Verify config validation and defaults

### REQ-2.3: Estimate Structure
Must provide:
- [ ] `bool ready` - enough history collected
- [ ] `bool stable` - jitter within tolerance
- [ ] `int64_t offset_mean_ns` - average offset
- [ ] `int64_t offset_stddev_ns` - offset standard deviation
- [ ] `double drift_ppm` - estimated frequency error
- [ ] `double drift_stddev_ppm` - drift standard deviation
- [ ] `double jitter_ns_rms` - RMS jitter (optional)
- [ ] `uint32_t health_flags` - health status bitmask
- **Test**: Verify estimate calculation correctness

### REQ-2.4: Health Flags
Must define:
- [ ] `HF_NOT_READY` - insufficient samples
- [ ] `HF_IN_HOLDOFF` - settling after event
- [ ] `HF_REFERENCE_BAD` - PPS source unreliable
- [ ] `HF_STEP_DETECTED` - clock discontinuity detected
- [ ] `HF_WINDOW_CONTAMINATED` - epoch boundary in window
- [ ] `HF_JITTER_TOO_HIGH` - excessive noise
- [ ] `HF_MISSING_TICKS` - PPS gaps detected
- **Test**: Verify flag setting conditions

### REQ-2.5: Observer Events
Must handle:
- [ ] `ReferenceChanged` - PPS source switched
- [ ] `ReferenceLost` - PPS missing/unreliable
- [ ] `ReferenceRecovered` - PPS restored
- [ ] `ClockStepped` - clock_settime, big offset correction
- [ ] `ClockSlewed` - small smooth correction
- [ ] `FrequencyAdjusted` - adjfreq/PI tuning change
- [ ] `ServoModeChanged` - free-run → disciplined transition
- [ ] `WarmStartRequested` - operator reset request
- **Test**: Verify each event triggers correct behavior

## 3. Ring Buffer Requirements

### REQ-3.1: Ring Buffer Behavior
- [ ] Fixed-size circular buffer of `DriftSample`
- [ ] Oldest samples overwritten when full
- [ ] Maintains up to `window_size` samples
- **Test**: Verify wraparound behavior

### REQ-3.2: Sample Storage
- [ ] Store raw timestamps (`t_ref_ns`, `t_clk_ns`)
- [ ] Store derived values (`offset_ns`, deltas, drift)
- [ ] Store validity and flags
- [ ] Enable debugging without recomputation
- **Test**: Verify all fields retrievable

## 4. Drift Estimation Requirements

### REQ-4.1: Method A - Robust Mean (Simple)
- [ ] Compute `drift[k] = offset[k] - offset[k-1]`
- [ ] Reject outliers using MAD or sigma clipping
- [ ] Output: `drift_ppm = mean(drift_valid) × 1e6 / 1e9`
- **Test**: Verify with known constant drift

### REQ-4.2: Method B - Linear Regression (Recommended)
- [ ] Fit `offset(t) ≈ a + b·t` using least squares
- [ ] Slope `b` is drift (seconds/second)
- [ ] Use sample_index as time (0..N-1)
- [ ] Operate on filtered/valid samples only
- **Test**: Verify regression accuracy with known slope

### REQ-4.3: Method Selection
- [ ] Use `Config.use_linear_regression` to choose method
- [ ] Linear regression is default/recommended
- **Test**: Verify both methods produce similar results for clean data

## 5. Spike Detection Requirements

### REQ-5.1: Offset Spike Detection
Detect when:
- [ ] `abs(offset[k] - offset[k-1]) > max_offset_step_ns`
- [ ] `abs(dt_ref - 1e9) > max_dt_ref_deviation_ns`
- [ ] Time step event notified
- **Test**: Verify offset jump detection

### REQ-5.2: Drift Spike Detection
Detect when:
- [ ] `abs(drift[k])` exceeds reasonable bounds
- [ ] Drift deviates from window median by MAD threshold
- **Test**: Verify drift outlier rejection

### REQ-5.3: Spike Handling
When spike detected:
- [ ] Mark sample as `valid = false`
- [ ] Set appropriate flag bits
- [ ] **Keep sample in buffer** for debugging
- [ ] Exclude from drift statistics
- **Test**: Verify spikes don't corrupt estimates

## 6. Epoch & Contamination Requirements

### REQ-6.1: Epoch Tracking
- [ ] Maintain `epoch_id` counter
- [ ] Store `epoch_id` per sample
- [ ] Increment epoch on contaminating events
- **Test**: Verify epoch boundaries

### REQ-6.2: Policy B - Reset Window (Recommended)
On contaminating event:
- [ ] Clear ringbuffer completely
- [ ] Increment `epoch_id`
- [ ] Start collecting fresh samples
- [ ] Become ready after `min_valid_samples`
- **Test**: Verify window reset behavior

### REQ-6.3: Contaminating Events
Trigger window reset:
- [ ] Clock stepped (`ClockStepped` event)
- [ ] Frequency adjusted (`FrequencyAdjusted` event)
- [ ] Reference changed (`ReferenceChanged` event)
- [ ] Automatic step detection threshold exceeded
- **Test**: Verify each event triggers reset

## 7. Holdoff/Settle Requirements

### REQ-7.1: Holdoff Timer
- [ ] Maintain `holdoff_ticks_remaining` counter
- [ ] Decrement each tick
- [ ] Set on contaminating events (configurable duration)
- **Test**: Verify holdoff countdown

### REQ-7.2: Holdoff Behavior
During holdoff:
- [ ] Accept samples into buffer (optional)
- [ ] Force `Estimate.ready = false` OR
- [ ] Force `Estimate.trustworthy = false`
- [ ] Set `HF_IN_HOLDOFF` flag
- **Test**: Verify estimates blocked during holdoff

### REQ-7.3: Holdoff Durations
- [ ] After PHC step: 3-10 seconds (configurable)
- [ ] After frequency adjust: 0-2 seconds (configurable)
- [ ] After reference change: 3-10 seconds (configurable)
- **Test**: Verify duration configuration

## 8. Trust Gating Requirements

### REQ-8.1: Ready Condition
`Estimate.ready = true` when:
- [ ] `valid_samples >= min_valid_samples`
- **Test**: Verify ready threshold

### REQ-8.2: Trustworthy Condition
`Estimate.trustworthy = true` when ALL:
- [ ] `valid_samples >= min_valid_samples`
- [ ] `invalid_ratio <= max_invalid_ratio` (e.g., ≤10%)
- [ ] `stddev(drift) <= max_drift_stddev_ppm`
- [ ] No step/epoch event in last `holdoff_ticks`
- [ ] Reference currently "good"
- **Test**: Verify trust gates

### REQ-8.3: Health Flags Accuracy
- [ ] Flags accurately reflect observer state
- [ ] Multiple flags can be set simultaneously
- [ ] Flags updated every tick
- **Test**: Verify flag combinations

## 9. API Requirements

### REQ-9.1: Constructor
- [ ] `DriftObserver(Config cfg)`
- [ ] Validate config parameters
- [ ] Allocate ringbuffer
- [ ] Initialize epoch_id = 0
- **Test**: Verify construction with valid/invalid config

### REQ-9.2: Update Method
- [ ] `void Update(int64_t t_ref_ns, int64_t t_clk_ns)`
- [ ] Called exactly once per PPS tick
- [ ] Compute offset, deltas, drift
- [ ] Detect spikes automatically
- [ ] Update ringbuffer
- [ ] Update statistics
- **Test**: Verify tick-by-tick behavior

### REQ-9.3: GetEstimate Method
- [ ] `Estimate GetEstimate() const`
- [ ] Return current estimate with all fields
- [ ] Non-blocking, const-correct
- **Test**: Verify estimate retrieval

### REQ-9.4: Latest Method
- [ ] `const DriftSample& Latest() const`
- [ ] Return most recent sample
- [ ] For debugging/logging
- **Test**: Verify latest sample access

### REQ-9.5: NotifyEvent Method
- [ ] `void NotifyEvent(ObserverEvent ev, int64_t magnitude_ns = 0)`
- [ ] Handle each event type appropriately
- [ ] Update epoch, reset window, set holdoff
- **Test**: Verify each event handling

## 10. Multi-Clock Requirements

### REQ-10.1: Independent Observers
- [ ] Multiple observers can run simultaneously
- [ ] Each observes different clock pair (GPS-RTC, GPS-PHC, etc.)
- [ ] All driven by same PPS tick
- **Test**: Verify multi-observer operation

### REQ-10.2: Observer Naming
- [ ] Support human-readable name (e.g., "GPS-RTC")
- [ ] Name used in logging/debugging
- **Test**: Verify observer identification

## 11. Automatic Detection Requirements

### REQ-11.1: Step Detection (Belt & Suspenders)
Even with explicit notifications:
- [ ] Detect if `abs(offset[k] - offset[k-1]) > step_threshold_ns`
- [ ] Detect if `abs(dt_clk - dt_ref) > dt_threshold_ns`
- [ ] Automatically trigger `ClockStepped` behavior
- **Test**: Verify automatic step detection

### REQ-11.2: Reference Quality Detection
- [ ] Detect if `dt_ref` deviates from 1 second
- [ ] Mark sample invalid
- [ ] Set `HF_REFERENCE_BAD` if persistent
- **Test**: Verify reference monitoring

### REQ-11.3: Tick Gap Detection
- [ ] Detect if Update() not called every second
- [ ] Mark gap samples invalid
- [ ] Set `HF_MISSING_TICKS`
- **Test**: Verify gap handling

## 12. Recommended Configuration (Defaults)

### REQ-12.1: Starter Profile
Defaults for GPS PPS + PHC/SYS on Raspberry Pi 5:
- [ ] `window_size = 120` (2 minutes)
- [ ] `min_valid_samples = 30` (30 seconds)
- [ ] `max_dt_ref_deviation_ns = 2000000` (2ms)
- [ ] `max_offset_step_ns = 1000000` (1ms)
- [ ] `outlier_mad_sigma = 4.5`
- [ ] `use_linear_regression = true`
- **Test**: Verify default configuration works

## 13. Observability Requirements

### REQ-13.1: Logging/Debug Output
Must be able to inspect:
- [ ] Latest offset
- [ ] Drift estimate ppm
- [ ] Stddev
- [ ] Number of valid samples
- [ ] Health flags (human-readable)
- **Test**: Verify debug output completeness

### REQ-13.2: Sample History Access
- [ ] Access to ringbuffer for debugging
- [ ] Iterate through recent samples
- [ ] Dump sample history on demand
- **Test**: Verify history retrieval

## Summary Statistics

**Total Requirements**: ~90+ testable requirements  
**Categories**: 13 major areas  
**Priority**: Core (1-5) > Advanced (6-11) > Config/Debug (12-13)

## TDD Test Plan

### Phase 1 (RED): Core Data Structures
- Test DriftSample population (REQ-2.1)
- Test Config validation (REQ-2.2)
- Test Estimate structure (REQ-2.3)

### Phase 2 (RED): Basic Operation
- Test ringbuffer behavior (REQ-3.1, 3.2)
- Test Update() mechanics (REQ-9.2)
- Test offset/drift calculation (REQ-1.2)

### Phase 3 (RED): Spike Detection
- Test offset spike detection (REQ-5.1)
- Test drift spike detection (REQ-5.2)
- Test spike exclusion (REQ-5.3)

### Phase 4 (RED): Drift Estimation
- Test Method A (REQ-4.1)
- Test Method B (REQ-4.2)
- Test method selection (REQ-4.3)

### Phase 5 (RED): Epoch & Contamination
- Test epoch tracking (REQ-6.1)
- Test window reset (REQ-6.2)
- Test contaminating events (REQ-6.3)

### Phase 6 (RED): Holdoff & Trust
- Test holdoff timer (REQ-7.1, 7.2)
- Test trust gating (REQ-8.1, 8.2)
- Test health flags (REQ-8.3)

### Phase 7 (RED): Event Handling
- Test NotifyEvent() (REQ-9.5)
- Test automatic detection (REQ-11.1, 11.2, 11.3)

### Phase 8 (GREEN): Implement Each Phase
- Implement minimum code to pass tests
- One phase at a time

### Phase 9 (REFACTOR): Optimize
- Performance optimization
- Code clarity
- Documentation
