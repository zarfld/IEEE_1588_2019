Below is an **updated, detailed specification** of the **3-step clock servo** using **Ubiquitous Language** only (no *PHASE_A / OPTION_1 / Stage A* etc.).
It is written so it can be dropped into your repo as an engineering spec.

---

# Specification: Three-Step Time Discipline Servo (Ubiquitous Language)

## 1. Purpose

The **Time Disciplinor** maintains a **Local Clock** (PHC) aligned to a **Reference Time** (GPS-UTC) by:

1. removing large offset once (**Alignment**)
2. measuring inherent frequency bias (**Bias Capture**)
3. continuously steering frequency to suppress drift (**Drift Tracking**)

It provides robust behavior under real hardware conditions (PPS dropout, NMEA jitter, reference loss), and avoids invalid calculations by ensuring **protected measurement windows**.

---

## 2. Ubiquitous Language

### 2.1 Time Entities

* **Reference Time**
  The external time source, expressed as UTC seconds + nanoseconds (GPS-UTC derived).

* **Local Clock**
  The PHC time (`/dev/ptpX`) being disciplined.

* **Time Offset** (`offset_ns`)
  The difference between Reference Time and Local Clock:
  `offset_ns = reference_time_ns - local_clock_ns`

### 2.2 Stability and Quality Concepts

* **Reference Association**
  The mapping between a specific PPS edge and the corresponding UTC second derived from NMEA (“this PPS marks the boundary of second X”).

* **Reference Association Locked**
  The system has consistent evidence that NMEA timestamps correctly label the PPS edge being used.

* **Pulse Continuity**
  Successive PPS pulses are received with no missing sequence increments.

* **Stable Reference**
  A reference state where:
  `Reference Association Locked` AND `Pulse Continuity` AND acceptable jitter bounds.

### 2.3 Servo Control Concepts

* **Alignment**
  A one-time correction that removes large Time Offset by stepping the Local Clock.

* **Frequency Bias** (`captured_frequency_bias_ppb`)
  The measured constant rate error of the Local Clock relative to the Reference Time (ppb).

* **Drift Rate** (`drift_rate_ppb`)
  The rate of change in Time Offset over time (ppb), computed from two offset samples separated by a known duration.

* **Frequency Steering** (`frequency_steering_ppb`)
  The applied frequency adjustment to the Local Clock to reduce drift and keep offset bounded.

### 2.4 Measurement Safety

* **Protected Measurement Window**
  A defined interval where **no stepping** occurs, and **frequency adjustments are held constant**, enabling valid drift calculations.

---

## 3. System Responsibilities

### 3.1 Reference Provider Responsibilities

The **Reference Provider** must:

* produce `reference_time_ns` (GPS-UTC)
* provide PPS sequence (`pps_seq`) and monotonic capture timestamps
* declare whether `Reference Association Locked` is true

### 3.2 Local Clock Adapter Responsibilities

The **Local Clock Adapter** must:

* read current `local_clock_ns`
* apply **Step Alignment** (set time) when commanded
* apply **Frequency Steering** (adjust frequency) within `max_adj_ppb` constraints

### 3.3 Servo Responsibilities

The **Time Disciplinor** must:

* decide when to align, when to measure bias, and when to steer drift
* protect measurement windows from invalidation
* detect and handle reference instability
* avoid frequent stepping during normal operation

---

## 4. High-Level Behavior

The Time Disciplinor has three operating modes:

1. **Acquire Alignment**
2. **Capture Frequency Bias**
3. **Track And Correct Drift**

These names are **domain meanings**, not implementation staging.

---

## 5. Operating Modes

## 5.1 Acquire Alignment (Offset Removal)

### Goal

Bring the Local Clock into the **Capture Range** around the Reference Time using a **single step**, then pause briefly to let the clock stabilize.

### Entry Conditions

Enter Acquire Alignment if any of the following apply:

* initial startup
* `abs(offset_ns)` exceeds `startup_step_threshold_ns`
* `abs(offset_ns)` exceeds `emergency_step_threshold_ns` during tracking
* reference recovered after loss and offset is too large for steering-only recovery

### Inputs

* `reference_time_ns`
* `local_clock_ns`
* reference quality state (`Stable Reference` true/false)

### Rules

1. If `Stable Reference` is **false**, stepping **may** be allowed on startup only if explicitly configured, otherwise defer.
2. If `abs(offset_ns)` > `startup_step_threshold_ns`:
   perform **Step Alignment** to `reference_time_ns`
3. After stepping: start a `stabilization_guard` interval, during which drift measurement is not allowed.

### Outputs

* Local Clock set to Reference Time once
* transition readiness for Bias Capture

### Postconditions

* Offset expected within `capture_range_ns` (not guaranteed instantly due to read timing)

---

## 5.2 Capture Frequency Bias (Baseline Measurement)

### Goal

Measure inherent Local Clock frequency bias relative to the Reference Time using a **protected window** and compute an initial `captured_frequency_bias_ppb`.

### Entry Conditions

Enter Bias Capture when:

* alignment completed OR offset already small
* `Stable Reference` is true
* `stabilization_guard` has expired

### Protected Window Rules

During Bias Capture:

* **No Step Alignment** is allowed.
* **Frequency Steering** must be held constant (initially 0 or previously applied value).
* Window duration is defined by `bias_capture_pulses` or `bias_capture_seconds`.

### Measurement Method (UL form)

1. Record a baseline sample at PPS edge `pps_seq_start`:

   * `t0_ref_ns` = derived reference time for this PPS edge
   * `t0_local_ns` = Local Clock timestamp captured at this same PPS edge

2. After `N` pulses, record second sample:

   * `t1_ref_ns`
   * `t1_local_ns`

3. Compute deltas:

   * `ref_delta_ns = t1_ref_ns - t0_ref_ns`
   * `local_delta_ns = t1_local_ns - t0_local_ns`

4. Compute rate error as bias:

   * `bias_ratio = (local_delta_ns - ref_delta_ns) / ref_delta_ns`
   * `captured_frequency_bias_ppb = bias_ratio * 1e9`

### Validity Checks

Bias Capture must be **rejected** if any occurs:

* PPS dropouts inside the window (`Pulse Continuity` violated)
* Reference Association becomes unlocked mid-window
* window duration is not within tolerance (unexpected delta)
* computed bias exceeds sanity bounds (`abs(ppm)` too large)

### Outputs

* `captured_frequency_bias_ppb` accepted and stored
* initial `frequency_steering_ppb = -captured_frequency_bias_ppb` may be applied immediately (optional policy)

### Postconditions

* servo transitions to drift tracking

---

## 5.3 Track And Correct Drift (Continuous Control)

### Goal

Keep Time Offset small by continuously estimating **Drift Rate** and applying **Frequency Steering**, while stepping only when absolutely required.

### Entry Conditions

Enter Drift Tracking when:

* bias capture is complete and valid
  OR
* a configuration allows entering tracking after alignment without bias capture (less accurate mode)

### Drift Rate Estimation (Best Practice)

Drift must be estimated from **two filtered offset samples** separated by a known time.

**Drift Sample A:**

* `offset_a_ns` measured and filtered at time `ta_ref_ns`

**Drift Sample B:**

* `offset_b_ns` measured and filtered at time `tb_ref_ns`

**Duration:**

* `dt_ns = tb_ref_ns - ta_ref_ns`

**Drift Rate:**

* `drift_rate_ppb = ((offset_b_ns - offset_a_ns) / dt_ns) * 1e9`

### Frequency Steering Law

The controller maintains a cumulative steering target:

* `frequency_steering_ppb = -(captured_frequency_bias_ppb + filtered_drift_rate_ppb)`

Where:

* `filtered_drift_rate_ppb` is drift_rate after smoothing
* `captured_frequency_bias_ppb` acts as baseline compensation

### Constraints

* Clamp to `max_adj_ppb` supported by the Local Clock:

  * `frequency_steering_ppb = clamp(frequency_steering_ppb, -max_adj_ppb, +max_adj_ppb)`

### When to Step During Tracking

**Do not step** during normal tracking.
Step only if:

* `abs(offset_ns) > emergency_step_threshold_ns`
* reference recovered after downtime and offset is huge
* drift tracking cannot stabilize due to repeated invalid reference

### Skipping Updates

Tracking loop must **skip steering updates** if:

* Reference Association is not locked
* Pulse continuity broken
* dt is too small or invalid
* offset sample is stale or untrusted

### Outputs

* steady `frequency_steering_ppb` adjustments
* bounded `offset_ns`

---

## 6. Filtering & Averaging (UL Compatible)

### 6.1 Offset Filtering

Offset values used for decisions may be filtered:

* median-of-k for outlier rejection
* EMA for smoothing

UL term: **Filtered Offset**

Required: filter must not introduce a delay that exceeds stability thresholds.

### 6.2 Drift Rate Filtering

Drift rate should be low-pass filtered:

* `filtered_drift_rate_ppb = EMA(drift_rate_ppb)`

UL term: **Smoothed Drift Rate**

---

## 7. Mode Transitions

### Acquire Alignment → Capture Frequency Bias

Transition when:

* `abs(filtered_offset_ns) < capture_range_ns`
* `Stable Reference == true`
* `stabilization_guard expired`

### Capture Frequency Bias → Track And Correct Drift

Transition when:

* bias capture window completes
* validity checks passed

### Track And Correct Drift → Acquire Alignment

Transition when:

* `abs(filtered_offset_ns)` exceeds `emergency_step_threshold_ns`
* or reference was lost and recovered and offset is too large for steering-only correction

---

## 8. Failure Handling Requirements

### 8.1 Reference Loss

If reference becomes unstable:

* do not step
* freeze steering at last good value (or decay slowly toward 0)
* remain in tracking but mark `Stable Reference = false`

When stable again:

* resume drift estimation
* if offset too large, re-enter alignment mode

### 8.2 PPS Dropouts

If dropouts occur:

* invalidate current measurement windows
* skip bias/drift updates until continuity restored
* optionally require `K` consecutive pulses before resuming

### 8.3 Time Discontinuity

If Local Clock time discontinuity detected (e.g. unexpected jump while not stepping):

* mark local clock unstable
* restart alignment process

---

## 9. Telemetry & Logs (UL Compliant)

### Must-have Metrics

* `reference_association_locked`
* `pulse_continuity_ok`
* `filtered_offset_ns`
* `captured_frequency_bias_ppb`
* `filtered_drift_rate_ppb`
* `frequency_steering_ppb`
* `local_clock_max_adj_ppb`
* measurement validity flags

### Example Log Lines (approved vocabulary)

* `[Discipline] Alignment applied: offset_ns=...`
* `[BiasCapture] Window started: start_seq=...`
* `[BiasCapture] Completed: bias_ppb=... accepted`
* `[BiasCapture] Rejected: reason=pulse_dropout`
* `[DriftTracking] offset_ns=... drift_ppb=... steer_ppb=...`
* `[Discipline] Step suppressed: reference unstable`
* `[Discipline] Recovery: reference stable again`

---

## 10. Compatibility With Your Design Intent (Best Practice Confirmation)

Your described sequence is **best practice**, and **compatible** with a robust implementation **if and only if** you enforce the UL rule:

✅ **Drift calculations must be performed inside a Protected Measurement Window.**
That means:

* do **not** apply Step Alignment inside the drift measurement window
* do **not** change steering mid-window
* measure two offsets separated by a known duration
* compute drift from the slope of offset change

This matches exactly what you wrote:

> “we MUST NOT adjust otherwise that calculation would be incorrect.”

Yes. That is the correct reasoning.

---

## 11. Implementation Checklist (Spec → Code)

### Required building blocks

* `ReferenceAssociation` module (PPS↔UTC mapping lock)
* `TimeOffsetEstimator` (read PHC, map to UTC edge, compute offset)
* `BiasCapture` (protected window baseline measurement)
* `DriftTracker` (two-point slope + filter)
* `TimeDisciplinor` (mode transitions + policy decisions)

### Required invariants

* Bias Capture cannot run unless reference is stable
* Drift estimation cannot run unless reference is stable
* No stepping during any protected window
* No measurement window survives PPS dropout
