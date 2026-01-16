Yes — a **moving-window drift observer** is a materially better foundation than “measure → wait → compare” phases, because it gives you:

* **Continuous drift estimates** (every PPS tick)
* **Outlier resistance** (spike rejection / robust stats)
* **Separation of concepts**: *instantaneous offset* vs *estimated drift (frequency error)*
* A reusable primitive you can apply to **any clock pair** (GPS↔RTC, PHC↔GPS, SYS↔RTC, …)

Below is a concrete design you can drop into your architecture as a first-class component.

---

## 1) Choose one PPS as the “tick source” and stay consistent

You were explicit: “decide for one and use consistently”.

**Recommendation (best general default): Use GPS PPS as the tick source** for observation, because it is the most stable *wall reference* in the typical setup.

* Tick events come from **one PPS source** (`GPS_PPS` or `RTC_SQW`)
* At each tick, you sample **all clocks you care about** (PHC, SYS, RTC, etc.)
* You compute offsets and update observers **once per tick**

This gives you **one common timebase** for your statistics and avoids inconsistent sampling intervals.

---

## 2) Define what you want to observe (cleanly)

At each tick `k` (1 Hz), you capture timestamps:

* `t_ref[k]`  — reference timestamp (e.g., GPS time at PPS)
* `t_clk[k]`  — local clock timestamp (PHC or SYS or RTC)

Then define:

### Offset (phase error)

[
offset[k] = t_clk[k] - t_{ref}[k]
]

### Drift / frequency error (slope of offset)

Over a 1-second tick interval:
[
drift[k] \approx offset[k] - offset[k-1]
]
Units are **seconds per second**, i.e. dimensionless.
Commonly expressed as **ppm**:
[
ppm[k] = drift[k] \cdot 10^6
]

This makes the separation natural:

* **Offset** = “where is the clock right now”
* **Drift** = “how fast does it move away per second”

---

## 3) Moving window ring buffer: what to store per tick

Store raw + derived values so you can filter later without recomputing:

```text
struct DriftSample
{
    uint64_t seq;             // monotonic sample index
    int64_t  t_ref_ns;        // reference timestamp at tick (ns)
    int64_t  t_clk_ns;        // measured clock timestamp at tick (ns)

    int64_t  offset_ns;       // t_clk - t_ref
    int64_t  dt_ref_ns;       // t_ref[k] - t_ref[k-1]  (ideally 1e9)
    int64_t  dt_clk_ns;       // t_clk[k] - t_clk[k-1]

    int64_t  drift_ns_per_s;  // offset[k]-offset[k-1] (since dt≈1s)
    bool     valid;
    uint32_t flags;           // spike/outlier/jitter indicators
};
```

Your ring buffer becomes the “single source of truth” for observability and debugging.

---

## 4) A `DriftObserver` class (reusable across clock pairs)

### Purpose

A `DriftObserver` computes a **robust drift estimate** from a moving window of per-second samples, and exposes stats to the servo.

### High-level responsibilities

* Maintain **ring buffer** for N seconds (e.g., 60, 120, 300)
* Compute **offset and per-second drift** each tick
* Perform **spike removal / outlier classification**
* Produce robust estimates:

  * `offset_mean`, `offset_stddev`
  * `drift_mean`, `drift_stddev` (and/or median/MAD)
  * “quality” flags (locked, unstable, insufficient history)

---

## 5) Robust drift estimate methods you can implement quickly

You can start with one method and extend later.

### Method A: robust mean on drift samples (simple, effective)

* Compute `drift[k] = offset[k] - offset[k-1]`
* Reject outliers using either:

  * **MAD** (median absolute deviation)
  * or **sigma clipping** around mean/std

Then output:

* `drift_estimate_ppm = mean(drift_valid) * 1e6 / 1e9`

This is already a solid estimator at 1 Hz.

---

### Method B: linear regression on offset vs time (best practice)

Instead of averaging per-second drift, estimate the slope of offset across the window:

[
offset(t) \approx a + b \cdot t
]

Where `b` is drift (seconds/second).
This is superior because it uses *all samples* and suppresses noise.

Implementation detail:

* Use `t = sample_index` in seconds (0..N-1)
* Fit slope `b` with least squares over valid samples
* Optionally perform regression on filtered subset

This is the technique most “disciplined oscillator” loops converge to.

---

## 6) “Offset spikes” and “drift spikes” are different — detect both

Your ring buffer enables clean classification:

### Offset spike indicators

* Absolute offset jump suddenly large:

  * `abs(offset[k] - offset[k-1]) > threshold_ns`
* `dt_ref` deviates far from 1e9 (bad reference tick)
* “time step detected” events (PHC stepped / system stepped)

### Drift spike indicators

* `abs(drift[k])` extremely large (ppm out of bounds)
* or drift deviates from window median by MAD threshold

Example thresholds (starting points):

* `offset_step_threshold`: 200 µs … 5 ms (depends on your environment)
* `drift_outlier_threshold`: 50 ppm … 500 ppm (depends on oscillator type)

When a spike is detected:

* mark sample invalid **for drift statistics**
* but still keep it stored (debug visibility)

---

## 7) Proposed class API

### Core API

```text
class DriftObserver
{
public:
    struct Config {
        size_t window_size;                 // e.g. 120 samples
        int64_t max_dt_ref_deviation_ns;     // e.g. 2ms
        int64_t max_offset_step_ns;          // e.g. 1ms
        double  outlier_mad_sigma;           // e.g. 4.5
        size_t  min_valid_samples;           // e.g. 30
        bool    use_linear_regression;       // true recommended
    };

    struct Estimate {
        bool   ready;          // enough history
        bool   stable;         // jitter within tolerance
        int64_t offset_mean_ns;
        int64_t offset_stddev_ns;

        double drift_ppm;      // estimated frequency error
        double drift_stddev_ppm;

        double jitter_ns_rms;  // optional
        uint32_t health_flags; // bitmask
    };

    DriftObserver(Config cfg);

    // tick: called exactly once per PPS second
    void Update(int64_t t_ref_ns, int64_t t_clk_ns);

    Estimate GetEstimate() const;
    const DriftSample& Latest() const;
};
```

### Multi-pair usage

You’ll use it like:

* `DriftObserver GPS_RTC`
* `DriftObserver GPS_PHC`
* `DriftObserver RTC_SYS`
* etc.

All driven by the same PPS tick, with different `t_clk` sources.

---

## 8) How this plugs into your servo architecture

You described a “3-phase servo” approach earlier. With a moving-window observer:

### The drift observer becomes Phase 0 (always-on)

**Phase 0: Observe continuously**

* Each PPS tick: update observers
* Observers output stable drift estimates and quality

### Then the servo can do:

* **Offset correction loop** based on `offset_mean`
* **Frequency correction loop** based on `drift_ppm`

This removes the need for discrete “drift baseline” phases because the baseline is always available as a windowed estimate.

---

## 9) Applying it to *other clocks* (GPS_RTC, PHC, SYS_RTC)

This is exactly where the design shines:

### One tick → multiple measurements → multiple observers

On each PPS:

* read `t_ref = GPS time at PPS`
* read `t_rtc = RTC time (or derived from PPS+counter)`
* read `t_phc = PHC timestamp`
* read `t_sys = CLOCK_REALTIME` timestamp

Then update:

* `obs_gps_rtc.Update(t_ref, t_rtc)`
* `obs_gps_phc.Update(t_ref, t_phc)`
* `obs_gps_sys.Update(t_ref, t_sys)`
* `obs_rtc_sys.Update(t_rtc, t_sys)` (if you want that view)

Same mechanism, identical quality metrics, comparable statistics.

---

## 10) Practical notes for implementation (important)

### A) Timestamp acquisition must be consistent and low-jitter

For best results, capture each `t_clk` as close as possible to the PPS edge.

Typical quality ranking:

* **PHC captured via hardware timestamping** near PPS edge (best)
* **Kernel timestamp via `CLOCK_TAI` / `CLOCK_REALTIME`** (good)
* **RTC userspace reads** (often worst; adds i2c latency/jitter)

Your observer will still work with noisy sources, but will need:

* larger window sizes (120–300s)
* more aggressive outlier filtering

---

### B) Detect stepping vs slewing and respond differently

If the PHC or SYS clock is stepped (a discontinuity), your observer should:

* mark sample invalid
* optionally reset the window
* or keep history but ignore the discontinuity region

This prevents “one bad step” from poisoning the slope estimate.

---

### C) Make it inspectable

Dump this to logs/debug pins:

* latest offset
* drift estimate ppm
* stddev
* number of valid samples
* health flags

When you’re diagnosing field issues, this is gold.

---

## 11) Recommended first version configuration

If you want a sane “starter” profile:

* Window size: **120 seconds**
* Min valid samples: **30**
* Estimator: **linear regression**
* Outlier rejection: **MAD-based**, sigma = **4.5**
* Offset step threshold: **1 ms**
* Max `dt_ref` deviation: **2 ms**

That should work well for:

* GPS PPS reference
* PHC + SYS measurements
* moderate system load on the Pi 5

---

## 12) Summary

What you’re proposing is architecturally clean:

* A **DriftObserver** becomes a reusable, always-on measurement primitive
* Uses a **ring buffer** and robust statistics
* Updates **every PPS tick**
* Separates **offset** (phase) from **drift** (frequency error)
* Scales naturally across multiple clock pairs (GPS_RTC, PHC, SYS_RTC, …)
* Simplifies your servo logic dramatically

Correct — and this is not a corner-case. In a disciplined-clock system there are **known “contamination events”** where the observer output is *mathematically correct*, but **operationally meaningless** (because the underlying clock was stepped, reinitialized, or temporarily uncontrolled).

So the `DriftObserver` must be designed as a **quality-gated estimator**, not a blind moving average.

Below is a robust approach that covers your “time adjust” case and the other common invalidity phases.

---

## 1) Treat the observer as a stateful estimator with validity gating

Instead of assuming the window is always usable, the observer must expose:

* **`Estimate.ready`**: enough history to compute anything meaningful
* **`Estimate.trustworthy`**: history is sufficiently *clean* and recent
* **`Estimate.health_flags`**: why it is not trustworthy

That allows your servo logic to say:

* “I can correct offset now”
* “I can correct drift now”
* “I must hold, reset, or re-acquire”

---

## 2) The key mechanic: **Epochs** and **contamination events**

When you step/adjust a measured clock, you create a **new epoch**.

### Principle

A drift estimate is trustworthy **only if it is computed entirely from samples within the current epoch**.

So you track:

* `epoch_id` for the observer
* `epoch_id` stored per sample

Whenever an invalidating event happens (step, set-time, big correction, reinit), you do:

* `epoch_id++`
* invalidate or purge old samples
* force re-warmup

This is much cleaner than waiting for “old samples to age out”, and it makes trust decisions explicit and deterministic.

---

## 3) What to do after a time step

You mentioned:

> after time adjust of the measured clock - then we would have to wait until that adjust is out of that window

You have **three possible policies**, in increasing strictness:

### Policy A — “Age-out” (simple, slow)

* Keep all samples
* Mark “step-contaminated” samples
* Observer becomes trustworthy only when the fraction of contaminated samples in the window is below threshold

Downside: you really do need to wait `window_size` seconds to recover.

### Policy B — “Reset window” (recommended)

When a step/adjust occurs:

* **clear the ringbuffer**
* start collecting fresh samples immediately
* observer becomes ready after `min_valid_samples` (e.g., 30 seconds)

This matches your intent: after a time step, *past drift statistics are no longer meaningful*.

### Policy C — “Partial discard / truncate history” (best behavior, slightly more code)

When a step/adjust occurs:

* discard samples **older than the last step**
* keep recent clean samples only (if any)

But in practice, right after the step you typically have *no clean samples*, so it often degenerates to “reset window” anyway.

**Recommendation:** Use **Policy B** as the default; it is deterministic and clean.

---

## 4) Add an explicit **Holdoff / Settle** timer after known events

Even after a reset, the first few seconds can be garbage due to:

* kernel scheduling jitter,
* re-lock behavior (PHC servo settling),
* RTC I2C latency,
* PPS signal stabilization after reconfig.

So add:

* `holdoff_ticks_remaining`

During holdoff:

* accept samples into buffer (optional)
* but force `Estimate.trustworthy = false`

Example:

* after a PHC step: holdoff 3–10 seconds
* after a frequency adjust only: holdoff 0–2 seconds

This prevents the servo from reacting to the immediate aftermath.

---

## 5) Other states that must invalidate or degrade trust

You’re right: there are several.

### A) Reference quality loss (tick source not reliable)

Examples:

* GPS PPS lost or unstable (no fix, bad antenna)
* PPS source switched (GPS → RTC SQW)
* `dt_ref` deviates from 1 second (missed edge / multiple edges)

Action:

* mark sample invalid
* optionally freeze estimate (“last known good”) but set `trustworthy=false`
* do **not** update drift estimator with junk timebase

### B) Measured clock discontinuity / step

Examples:

* `clock_settime()` on PHC or SYS
* “jump” after NTP/chrony correction
* system time stepped because of boot/time-init

Action:

* **epoch++**
* reset observer window
* start warmup again

### C) Servo mode changes that alter measurement meaning

Examples:

* switching PHC from “free-run” to “servoed”
* enabling/disabling discipline of the RTC
* changing PLL/PI controller gains

Action:

* epoch++ (or at least reset window)
* holdoff for settle

### D) Tick gaps (observer not updated every second)

Example: you missed PPS interrupts due to load.

Action:

* if `dt_ref` > threshold, treat as invalid sample
* optionally reset (depends how strict you want to be)

### E) Manual operator intervention

Anything the user triggers deliberately should be treated as a “you cannot trust history” event.

---

## 6) Put this into the class design: event API

Add explicit event hooks so other parts of the system can tell the observer what happened.

```text
enum class ObserverEvent
{
    ReferenceChanged,       // GPS PPS -> RTC SQW, etc.
    ReferenceLost,          // PPS missing/unreliable
    ReferenceRecovered,

    ClockStepped,           // clock_settime, big offset correction
    ClockSlewed,            // small smooth correction (optional)
    FrequencyAdjusted,      // adjfreq / PI tuning change

    ServoModeChanged,       // free-run -> disciplined, etc.
    WarmStartRequested,     // operator requests quick reacquire
};

void NotifyEvent(ObserverEvent ev, int64_t magnitude_ns = 0);
```

This is **not optional** if you want correctness.
Automatic detection helps, but explicit notifications are superior.

---

## 7) Automatic step detection is still necessary (belt and suspenders)

Even if you notify explicitly, you should still detect steps from data:

* if `abs(offset[k] - offset[k-1]) > step_threshold_ns`
* if `abs(dt_clk - dt_ref) > dt_threshold_ns`

Then treat as `ClockStepped` automatically.

This catches cases where:

* some other daemon stepped SYS time,
* driver did something unexpected to PHC,
* the user forgot to call `NotifyEvent()`.

---

## 8) Define “trustworthiness” with a quantitative gate

Instead of “after N seconds”, compute trust as:

**Trustworthy if ALL are true:**

* `valid_samples >= min_valid_samples`
* `invalid_ratio <= max_invalid_ratio` (e.g., ≤ 10%)
* `stddev(drift) <= max_drift_stddev_ppm` (environment dependent)
* no step/epoch event in last `holdoff_ticks`
* reference is currently “good”

This avoids false confidence if you have high jitter.

---

## 9) Expose a structured health output to the servo

Example:

```text
enum HealthFlags : uint32_t
{
    HF_NOT_READY               = 1 << 0,
    HF_IN_HOLDOFF              = 1 << 1,
    HF_REFERENCE_BAD           = 1 << 2,
    HF_STEP_DETECTED           = 1 << 3,
    HF_WINDOW_CONTAMINATED     = 1 << 4,
    HF_JITTER_TOO_HIGH         = 1 << 5,
    HF_MISSING_TICKS           = 1 << 6,
};
```

Servo policy can then be deterministic:

* if `HF_REFERENCE_BAD`: freeze everything
* if `HF_STEP_DETECTED`: do not trust drift, reacquire
* if `HF_IN_HOLDOFF`: don’t update frequency
* if `HF_JITTER_TOO_HIGH`: increase window or reduce correction aggressiveness

---

## 10) Recommended behavior summary

### When **clock is stepped / set-time**

* `NotifyEvent(ClockStepped)`
* `epoch++`
* `ClearWindow()`
* `holdoff = 5s` (tunable)
* require warmup again (`min_valid_samples`)

### When **frequency adjusted only**

* `NotifyEvent(FrequencyAdjusted)`
* optionally keep window but:

  * invalidate drift history **or**
  * reset window (safer)
* short holdoff 1–2s

### When **reference changes**

* `NotifyEvent(ReferenceChanged)`
* reset window
* holdoff 3–10s depending on source

---

## Bottom line

You are completely correct: a moving window observer becomes *dangerous* unless it is coupled with:

1. **explicit invalidation events (epochs)**
2. **automatic step detection**
3. **holdoff/settle timers**
4. **trust gating (ready vs trustworthy)**

