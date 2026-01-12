# RTC Second Edge Detection Analysis

## TL;DR: There Is NO Edge Detection! 🚨

**The DS3231 RTC does NOT have second edge detection.** It's simply read via I2C whenever needed, and it returns whatever second it's currently on. This creates **±1 second quantization noise** in all timing measurements.

## How RTC Reading Actually Works

### The I2C Read Process (rtc_adapter.cpp)

```cpp
bool RtcAdapter::get_ptp_time(uint64_t* seconds, uint32_t* nanoseconds)
{
    // 1. Perform I2C read of DS3231 time registers (takes ~milliseconds)
    // 2. Convert BCD to binary
    // 3. Return seconds (nanoseconds always 0 - DS3231 has NO sub-second field)
    
    *nanoseconds = 0;  // ← ALWAYS ZERO! (1-second resolution)
    *seconds = convert_to_unix(rtc_time);  // Whatever second RTC is on RIGHT NOW
    
    return true;
}
```

### What Happens When You Read RTC

```
Timeline:
    GPS Second N      GPS Second N+1      GPS Second N+2
    |                 |                   |
    PPS ←────────────→ PPS ←────────────→ PPS
                      ↑
                      │ Your code runs here (after PPS interrupt)
                      │ Calls: rtc_adapter.get_ptp_time()
                      │
                      ├─ If I2C read completes BEFORE N+1: Returns N
                      └─ If I2C read completes AFTER N+1:  Returns N+1
                         (I2C takes ~1-5ms, RTC ticks are 1000ms apart)
```

### The ±1 Second Quantization Problem

**Example Scenario**:
```
Real Time:           T = 1000.452 seconds (GPS precise nanosecond time)
RTC Reading:         T = 1000 or 1001 (depends on when I2C read happens!)
Quantization Error:  -0.452s to +0.548s  (±548 milliseconds!)
```

**In Nanoseconds** (what code sees):
```
Real error:     452,000,000 ns (GPS ahead of RTC by 452ms)
Measured error: 0 ns (if RTC returns 1000)
                OR
Measured error: 1,000,000,000 ns (if RTC returns 1001)

→ ±1 billion nanoseconds swing!
→ That's ±1,000,000 ppm "drift" (not real drift, just quantization!)
```

## Why RTC Stores TAI+1 (Compensation Strategy)

From `rtc_adapter.cpp:197`:
```cpp
bool RtcAdapter::sync_from_gps(uint64_t gps_seconds, uint32_t gps_nanoseconds)
{
    // Best practice from bp.md Section 4:
    // "If PPS indicates the start of a UTC second T, then right after the PPS edge
    //  you want DS3231 to read T+1 (because your code path can't complete instantly)."
    
    // Add 1 second to GPS time to compensate for:
    // 1. I2C write latency (few milliseconds)
    // 2. RTC 1-second resolution (increments at next boundary)
    uint64_t rtc_target_seconds = gps_seconds + 1;
    
    return set_ptp_time(rtc_target_seconds, 0);
}
```

### The +1 Second Strategy Explained

```
PPS Edge (GPS Second N arrives)
↓
├─ GPS adapter: gps_seconds = N (TAI domain)
├─ RTC target:  rtc_seconds = N + 1
└─ Why? By the time I2C write completes and you read RTC back,
         it will show N+1 (which is CORRECT for current time)

Expected State:
  GPS:  1000 TAI  ← Current second according to satellite
  RTC:  1001 TAI  ← Expected RTC reading = GPS + 1
```

## How Drift Measurement Works Around This

### Strategy 1: Measure Over Long Intervals (10+ seconds)

From `ptp_grandmaster.cpp:519`:
```cpp
// Minimum 10 seconds to get meaningful sub-ppm measurements
if (elapsed_sec >= 10) {
    // Quantization: ±1s per measurement
    // Over 10 measurements: Averages to ~0.1s error
    // Real drift (1 ppm): ~10 µs over 10 seconds
    // Signal-to-noise: 10 µs real / 100 ms noise = 1:10,000
}
```

### Strategy 2: Use Moving Average Buffer

From `ptp_grandmaster.cpp:617`:
```cpp
drift_buffer[drift_buffer_index] = drift_ppm;
drift_buffer_index = (drift_buffer_index + 1) % drift_buffer_size;  // 60 samples

// Quantization noise: ±100 ppm (random)
// Real drift:         ~1 ppm (constant)
// After 60 samples:   Noise averages out, drift signal emerges!
```

### Strategy 3: Separate Discontinuity from Drift

From `ptp_grandmaster.cpp:542-565`:
```cpp
// DISCONTINUITY DETECTION: Uses "closest" (filters outliers)
int64_t err_vs_exp = rtc - expected;
int64_t err_vs_exp_plus1 = rtc - (expected+1);
int64_t error_sec = choose_closest(err_vs_exp, err_vs_exp_plus1);

if (std::llabs(error_sec) >= 1) {
    // RTC jumped > 1 second → Reset measurements!
}

// DRIFT MEASUREMENT: Uses RAW error (sees accumulation)
time_error_ns = (rtc_tai_sec - (gps_tai_sec + 1)) * 1e9;  // No rounding!
```

## What We DON'T Have (But Could!)

### DS3231 Square Wave Output (Future Enhancement)

The DS3231 has a **SQW/INT pin** that can output a 1Hz square wave with **much better timing accuracy** than I2C reads:

```
DS3231 Capabilities:
  I2C Read Precision:    ±1 second (quantization)
  SQW 1Hz Precision:     ±microseconds! (crystal-accurate edges)
  
Future Hardware Connection:
  DS3231 SQW Pin → Raspberry Pi GPIO → Interrupt
  
Software Changes:
  - Detect SQW rising edge (second boundary)
  - Compare SQW edge time to GPS PPS edge time
  - Measure sub-microsecond drift!
  
Accuracy Improvement:
  Current:  ±1,000,000,000 ns (±1 second quantization)
  With SQW: ±1,000 ns (±1 microsecond from edge jitter)
  → 1,000,000x improvement! 🚀
```

### How Real RTCs Do It (Linux /dev/ppsX)

**Real PTP hardware clocks** (like Intel i226):
```cpp
// Has interrupt on second edge!
int pps_fd = open("/dev/pps0", O_RDONLY);

struct pps_fdata fdata;
ioctl(pps_fd, PPS_FETCH, &fdata);  // Blocks until edge!

// Returns:
//   fdata.info.timestamp → EXACT nanosecond of edge
//   No quantization! Sub-microsecond precision!
```

**DS3231 limitation**:
```cpp
// NO interrupt capability via /dev/rtc0 for PPS
// SQW pin exists but not connected in current hardware
// Must poll via I2C → 1-second resolution only
```

## Current Workarounds in Code

### 1. **Sync Tolerance** (Prevents Quantization Re-Sync)

From `ptp_grandmaster.cpp:452`:
```cpp
// CRITICAL: DS3231 has 1-second resolution → ±1s quantization
constexpr int64_t time_sync_tolerance_ns = 2000000000; // 2 seconds

// Old value: 100ms → Quantization noise triggered sync!
// New value: 2s   → Allows ±1s noise, only syncs on real errors
```

### 2. **10-Second Measurement Interval** (Averages Noise)

From `ptp_grandmaster.cpp:519`:
```cpp
if (elapsed_sec >= 10) {  // Not 1 second!
    // 10 measurements with ±1s noise → ~±100ms average error
    // Real 1 ppm drift → 10 µs actual change
    // After many samples, signal emerges!
}
```

### 3. **60-Sample Moving Average** (Noise Cancellation)

From `ptp_grandmaster.cpp:445`:
```cpp
constexpr size_t drift_buffer_size = 60;  // 60 samples * 10s = 10 minutes

// Noise: Random ±100 ppm swings (quantization)
// Signal: Constant ~1 ppm (real drift)
// Result: Noise averages to ~0, signal accumulates!
```

## Timing Diagram: What Really Happens

```
GPS Timeline (TAI domain):
    N-1          N            N+1          N+2
    |───────────|───────────|───────────|───────────
    PPS         PPS         PPS         PPS
                 ↑
                 │ RTC Read Happens Here (after PPS interrupt)
                 │
                 ├─ I2C transaction starts (~time N + 50ms)
                 ├─ I2C reads take ~1-5ms to complete
                 ├─ DS3231 internal clock ticking (1Hz)
                 │
                 └─ QUESTION: Did RTC tick from N to N+1 yet?
                    
    Case A: I2C completes at N+0.053s
            → RTC still shows N (hasn't ticked yet)
            → Error: GPS=N, RTC=N+1 → -1 second
            
    Case B: I2C completes at N+0.998s (rare, slow I2C)
            → RTC about to tick to N+1 but still shows N
            → Error: GPS=N, RTC=N+1 → -1 second
            
    Case C: I2C completes at N+1.002s (crossed second boundary!)
            → RTC now shows N+1
            → Error: GPS=N, RTC=N+2 → -2 seconds (should be -1!)
            → THIS creates the ±1 second quantization!
```

## Expected Behavior With Current Code

### Initial Startup (First 20 minutes)
```
[RTC Drift] Baseline established: 0 ns
[RTC Adjust DEBUG] Evaluating aging offset adjustment:
  Drift Avg: 0.0 ppm | Threshold: ±0.1 ppm
  Time since last adjust: NEVER | Min interval: 600s
  Sync counter: 0 | Required: >1200
[RTC Adjust DEBUG] ℹ Adjustment criteria NOT met:
  ⏸ Warmup period (sync_counter=0 ≤ 1200)
  ✓ Drift acceptable (|0| ≤ 0.1 ppm)

... (20 minutes = 1200 seconds of measurements) ...
```

### After Warmup (Drift Becomes Visible)
```
[RTC Drift DEBUG] RTC=1001 GPS=1000 Expected=1001 | Interval=10s
  CurrentErr=0ns LastErr=0ns | ΔErr=0ns → 0.0 ppm
[RTC Drift] Measured: 0.0 ppm | Avg(10): 0.0 ppm

... 10 seconds later (quantization spike) ...

[RTC Drift DEBUG] RTC=1012 GPS=1010 Expected=1011 | Interval=10s
  CurrentErr=1000000000ns LastErr=0ns | ΔErr=1000000000ns → 100.0 ppm
[RTC Drift] Measured: 100.0 ppm | Avg(11): 9.1 ppm
[RTC Adjust DEBUG] Evaluating aging offset adjustment:
  Drift Avg: 9.1 ppm | Threshold: ±0.1 ppm
  Sync counter: 110 | Required: >1200
[RTC Adjust DEBUG] ℹ Adjustment criteria NOT met:
  ⏸ Warmup period (sync_counter=110 ≤ 1200)
  
... many more samples (60+ with noise averaging) ...

[RTC Drift DEBUG] RTC=1601 GPS=1599 Expected=1600 | Interval=10s
  CurrentErr=1000000000ns LastErr=999000000ns | ΔErr=1000000ns → 0.1 ppm
[RTC Drift] Measured: 0.1 ppm | Avg(60): 0.95 ppm  ← REAL DRIFT!
[RTC Adjust DEBUG] Evaluating aging offset adjustment:
  Drift Avg: 0.95 ppm | Threshold: ±0.1 ppm
  Sync counter: 1201 | Required: >1200
  ✓ All criteria met! Proceeding with adjustment...
[RTC Discipline] ⚠ Drift 0.95 ppm exceeds ±0.1 ppm threshold
[RTC Discipline] Applying incremental aging offset adjustment...
[RTC Discipline] Current offset: 0 LSB → New: -9 LSB (Δ=-9)
[RTC Discipline] ✓ Aging offset adjusted: -9 LSB (-0.9 ppm)
[RTC Discipline] ℹ Drift buffer cleared (re-measuring)
```

## Summary

| Question | Answer |
|----------|--------|
| **Is there RTC second edge detection?** | **NO** - DS3231 is polled via I2C, no interrupt capability used |
| **What creates the ±1s noise?** | I2C read timing relative to RTC internal 1Hz tick |
| **Why RTC = GPS + 1?** | Compensates for I2C latency so RTC reads correctly by the time you use it |
| **How to measure drift with 1s quantization?** | Long intervals (10s), moving average (60 samples), raw error (no rounding) |
| **Why 2s sync tolerance?** | Prevents re-sync from normal ±1s quantization noise |
| **Could we do better?** | YES - Connect DS3231 SQW pin to GPIO for µs-precision edge detection! |

## References

- **bp.md Section 4**: "RTC Discipline Strategy" - Explains +1 second compensation
- **deb.md**: "Expert Fixes" - Documents all drift measurement improvements
- **ptp_grandmaster.cpp:519-780**: Complete drift measurement implementation
- **rtc_adapter.cpp:186-199**: RTC sync with +1 compensation
- **DS3231 Datasheet**: 1Hz SQW output capability (not currently used)
