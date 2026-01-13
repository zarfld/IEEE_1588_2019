# RTC Discipline Improvements

**Based on**: deb.md expert analysis  
**Date**: 2026-01-13  
**Status**: ⏳ Implementation in Progress

## Overview

This document tracks improvements to the DS3231 RTC aging offset discipline system based on expert analysis of the GPS PPS/RTC drift measurements.

## Expert Analysis Summary (from deb.md)

### Current System Behavior ✅ WORKING CORRECTLY

1. **GPS PPS jitter**: 0.5–3.0 µs peak-to-peak (normal for Linux userspace without extreme RT tuning)
2. **RTC PPS fetch**: Correctly detects new edges (`seq=...` increments each second)
3. **Drift estimation**: Working - values bounce around (±1 ppm) at 10s windows as expected
4. **Aging offset correction**: Directionally correct and proportionate:
   - Measured: `+0.176 ppm` average drift
   - Applied: `-2 LSB` correction (~`-0.2 ppm`)
   - Result: New average near `0 ppm` (within threshold)

### What the ~1.6ms "Error" Actually Means

**NOT a drift** - This is the **userspace wakeup latency**:
- Timestamp offset from PPS edge when we sample `now_utc_ns`
- Dominated by scheduling latency (not RTC error)
- Used correctly for ΔErr/Δt drift calculations
- **Should be kept separate from frequency discipline metrics**

### Noise vs. Instability

The ±1 ppm swings per 10s window are **measurement noise**, not DS3231 instability:
- 10s windows + variable scheduling latency (few µs)
- Trying to regulate to ±0.100 ppm threshold
- Over longer windows, average converges correctly

---

## Implementation Recommendations

### A) Increase Averaging Window (CRITICAL)

**Problem**: Single-window noise spikes (±1 ppm) with threshold at ±0.1 ppm causes unnecessary aging adjustments.

**Solutions**:

#### Option 1: Longer Effective Averaging
- **N ≥ 120 samples × 10s** = 20 minutes minimum
- OR compute drift from **linear regression** over 10–30 minutes of `(t, err)` data

#### Option 2: Stability Gates
- Require `abs(drift_avg) > threshold` **AND** `stddev(drift) < X`
- Use median absolute deviation (MAD) for outlier rejection

**Implementation Priority**: HIGH (reduces false threshold crossings)

---

### B) Don't Mix Offset with Frequency in Logs (MODERATE)

**Problem**: Logs show "Error: 1.64 ms" next to "Drift: 0.392 ppm", confusing two different control problems.

**Solution**: Separate log lines:
```
[Phase Monitor] Absolute offset from PPS edge: 1.642 ms (latency/phase metric)
[Frequency Servo] Drift: 0.392 ppm (3922ns/10s) | Avg(24): 0.392 ppm | Threshold: ±0.100 ppm
```

**Implementation Priority**: MODERATE (readability improvement)

---

### C) Fix Timestamping Path for Lower Latency (OPTIONAL)

**Goal**: Reduce the ~1.6ms offset (if phase accuracy matters).

**Solutions**:
1. Use **kernel PPS timestamps** directly (already fetched)
2. Avoid recomputing "now" with additional `clock_gettime`
3. Measure **PPS-to-PPS** deltas instead of **PPS vs now**

**Note**: This doesn't affect frequency discipline (already using ΔErr correctly).

**Implementation Priority**: LOW (cosmetic - doesn't impact drift measurements)

---

### D) Pin and Prioritize PPS Thread ✅ COMPLETE (2026-01-13)

**Goal**: Reduce drift noise → fewer unnecessary aging adjustments.

**Implementation Status**: ✅ RT thread pinned to CPU2 with SCHED_FIFO priority 80

**Code Location**: `src/ptp_grandmaster.cpp`, lines 363-420

**Thread Architecture**:
- **RT Thread**: CPU2, SCHED_FIFO priority 80, PPS edge detection + PHC sampling
- **Worker Thread**: CPU0/1/3, normal priority, GPS/RTC/PTP operations
- **Synchronization**: Mutex-protected shared data structure

**System Configuration Required** (USER ACTION):
```bash
# Add to /boot/firmware/cmdline.txt
isolcpus=2 nohz_full=2 rcu_nocbs=2

# Optional: Disable frequency scaling on CPU2
echo performance > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor

# Optional: Disable deep C-states
echo 1 > /sys/devices/system/cpu/cpu2/cpuidle/state*/disable

# Reboot required
```

**Expected Benefits**:
- Reduce PPS jitter: 0.5-3.0µs → <500ns
- Reduce drift noise: ±1ppm → ±0.2ppm
- 50-75% fewer false aging offset adjustments

**Validation**: Requires 24-hour hardware test with oscilloscope jitter measurement

---

### E) Explicit Proportional Control Law (MODERATE)

**Current**: Fixed ±1 or ±2 LSB steps  
**Recommended**: Proportional correction based on measured drift

**Implementation**:
```cpp
// Calculate proportional correction
double ppm_per_lsb = 0.1;  // DS3231 spec
int8_t delta_lsb = (int8_t)round(drift_avg_ppm / ppm_per_lsb);

// Clamp to reasonable range
if (delta_lsb > 3) delta_lsb = 3;
if (delta_lsb < -3) delta_lsb = -3;

// Apply only if drift exceeds threshold AND system is stable
if (abs(drift_avg_ppm) > threshold_ppm && stddev(drift) < stability_threshold) {
    int8_t new_offset = current_offset - delta_lsb;  // Negative because DS3231 inverts
    write_aging_offset(new_offset);
}
```

**Benefits**:
- Better convergence (larger corrections when far from target)
- Clearer control behavior (explicit proportionality)
- Stability gate prevents oscillations

**Implementation Priority**: MODERATE (nice-to-have improvement)

---

## Implementation Plan

### Phase 1: Quick Wins ✅ COMPLETE
- [x] Step 1: Missed PPS detection (deb.md)
- [x] Step 2: Servo state machine (deb.holdover.md)

### Phase 2: Averaging and Stability ✅ COMPLETE (2026-01-13)
- [x] Recommendation A: Increase averaging window to 120 samples (20 min)
- [x] Recommendation A: Add stddev stability gates (0.3 ppm threshold)
- [x] Recommendation E: Implement explicit proportional control (±3 LSB clamp)

### Phase 3: Logging and Diagnostics ✅ COMPLETE (2026-01-13)
- [x] Recommendation B: Separate offset/frequency logs
- [ ] Add detailed drift histogram tracking (optional)
- [ ] Create dashboarding script for long-term monitoring (optional)

### Phase 4: Performance Optimization ✅ COMPLETE (2026-01-13)
- [x] Recommendation D: Thread pinning and RT priorities (CPU2, SCHED_FIFO 80)
- [ ] Recommendation C: Optimize timestamping path (LOW PRIORITY - deferred)
- [ ] Benchmark jitter reduction from RT tuning (requires 24-hour test)

---

## Testing Strategy

### Unit Tests
```cpp
// Test averaging window calculation
void test_drift_averaging_120_samples();

// Test stability gate logic
void test_stddev_gate_rejects_noisy_data();

// Test proportional control law
void test_proportional_aging_offset_calculation();

// Test thread configuration
void test_pps_thread_priority_and_affinity();
```

### Integration Tests
```cpp
// Verify 20-minute averaging convergence
void test_long_averaging_reduces_false_triggers();

// Verify stability gates prevent oscillations
void test_stability_gate_prevents_premature_adjust();

// Verify proportional control improves convergence
void test_proportional_control_faster_lock();
```

### Hardware Tests (Raspberry Pi)
1. Run for 24 hours with GPS/RTC discipline
2. Measure aging offset adjustment frequency
3. Compare before/after jitter statistics
4. Validate drift convergence within ±0.1 ppm

---

## Metrics and Success Criteria

### Before Improvements (Baseline)
- Averaging window: 10 seconds
- Aging offset adjustments: Every ~10 minutes (too frequent)
- Drift noise: ±1 ppm swings per window
- Phase offset: ~1.6 ms (latency-dominated)

### After Improvements (Target)
- Averaging window: ≥20 minutes with stability gates
- Aging offset adjustments: Every 1-2 hours (stable)
- Drift noise: ±0.2 ppm (RT tuning)
- Phase offset: <500 µs (timestamping improvements)
- Long-term stability: ±0.05 ppm over 24 hours

---

## References

- **deb.md**: Expert analysis of console.log drift measurements
- **deb.holdover.md**: RTC holdover and servo state machine
- **deb.fefinement.md**: Hardware register-level refinements
- **console.log**: 24-hour GPS/RTC discipline log data
- **IMPLEMENTATION_PLAN.md**: Overall project status and integration

---

## Related GitHub Issues

- #TBD: Longer averaging windows for RTC discipline
- #TBD: Separate offset/frequency logging
- #TBD: PPS thread pinning and RT priorities
- #TBD: Explicit proportional control law
- #TBD: Timestamping path optimization

---

**Next Actions**:
1. Create GitHub issues for each recommendation
2. Implement Recommendation A (averaging) + E (proportional) first
3. Validate with 24-hour hardware test
4. Apply Recommendation D (RT tuning) for noise reduction
5. Document results and update thresholds
