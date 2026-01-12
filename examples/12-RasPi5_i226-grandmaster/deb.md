Understood. A separated-thread design is the right approach here, because you have **three different timing regimes** with very different latency sensitivity:

* **PHC discipline + PTP traffic**: hard latency requirements (sub-ms scheduling jitter matters).
* **PPS association / cross-timestamp**: must run “immediately after PPS” for clean measurements.
* **RTC discipline**: slow control loop (seconds to minutes), must be stable and stateful, not “fast”.

Below is a concrete architecture that keeps **PHC** and **RTC** both disciplined, while ensuring that **no blocking GPS/USB I/O can ever delay PPS→PHC sampling**.

---

## 1) Threading model (recommended)

### Thread A — `pps_phc_rt` (SCHED_FIFO, highest prio)

**Purpose:** deterministically handle PPS, take PHC samples, produce “time observations” for PHC servo and for UTC mapping.

**Hard rules**

* Must not do any blocking I/O except *waiting for PPS*.
* Must not parse NMEA.
* Must not talk to I2C RTC.
* Must not allocate memory (preallocate everything).

**Inputs**

* `/dev/pps0` (PPS events)
* `/dev/ptp0` (PHC time queries + adjustments)
* “latest UTC label” published by GPS thread (atomic struct)

**Outputs**

* PHC servo updates (frequency/phase corrections)
* Observation stream for RTC thread (e.g., `(pps_seq, phc_ns_at_pps, utc_sec_label, quality)`)

**CPU/affinity**

* Pin to your isolated core (you used core 2): `pthread_setaffinity_np()`.
* Use `mlockall(MCL_CURRENT|MCL_FUTURE)` at process start.
* Consider `sched_setattr`/`SCHED_FIFO` and keep this thread’s work bounded.

---

### Thread B — `gps_io` (normal priority or low FIFO)

**Purpose:** read GPS data (USB CDC-ACM), parse NMEA/UBX, maintain “UTC labeling” and GPS status.

**Hard rules**

* It may block; that’s fine.
* It must never block Thread A.

**Inputs**

* `/dev/ttyACM0` (NMEA or UBX), potentially GNSS status messages

**Outputs**

* Atomic “GPS state” + “current UTC second/date” + “confidence”
* Optional: ring-buffer of last N parsed sentences for diagnostics

**Implementation notes**

* Use `poll()` on the serial FD and read in bursts.
* Reduce the NMEA talker load (only RMC + maybe GGA) if you can configure the receiver.

---

### Thread C — `rtc_discipline` (normal priority)

**Purpose:** discipline RTC (DS3231) as a **fallback timebase** when GPS is lost, and as a long-term stable holdover reference.

**Hard rules**

* Never touches PHC control knobs directly.
* Runs slow (e.g., 1 Hz sampling, but control actions every 30–300 s).

**Inputs**

* Observation stream from Thread A (ideally via lock-free ring buffer)
* GPS state from Thread B (lock status, fix quality, etc.)
* `/dev/rtc1` + `/dev/i2c-*` for DS3231

**Outputs**

* RTC time set on (re-)lock events
* DS3231 aging offset updates (small incremental steps, hysteresis)
* “holdover status” and RTC quality estimate, for the main state machine

---

### Thread D — `ptp_net_txrx` (optional split)

If your PTP network traffic involves any sockets that might block or wake spuriously, it can be beneficial to isolate it:

* Thread A produces timestamps / Sync scheduling decisions.
* Thread D handles packet I/O and timestamp retrieval.
  Often you can keep it in Thread A if it’s fully non-blocking and bounded, but on Linux it’s usually cleaner to isolate.

---

## 2) Data exchange: keep it lock-free and bounded

### GPS → PPS/PHC thread: atomic “UTC label”

A single struct updated by Thread B:

```c
typedef struct {
  _Atomic uint64_t gen;
  _Atomic int gps_locked;        // 0/1
  _Atomic int fix_quality;       // NMEA quality
  _Atomic uint64_t utc_sec;      // UTC epoch second for “LAST PPS”
  _Atomic int utc_valid;         // 0/1
  _Atomic int64_t nmea_to_pps_ns_est; // optional (for association diagnostics)
} gps_label_t;
```

Use a generation counter (`gen`) pattern: writer increments before+after write to avoid tearing.

### PPS/PHC → RTC thread: ring buffer of observations

Each PPS produces a compact record:

```c
typedef struct {
  uint64_t pps_seq;
  int gps_locked;
  uint64_t utc_sec;          // if available
  uint64_t phc_ns_at_pps;    // PHC time sampled immediately after PPS
  uint32_t flags;            // association ok, jitter flags, etc.
} pps_obs_t;
```

RTC thread consumes this at its own pace; if it falls behind, it can drop old samples (bounded buffer) rather than stalling Thread A.

---

## 3) State machine: ensure both PHC and RTC get disciplined properly

### States (practical)

* **`GPS_LOCKED`**: GPS+PPS valid; PHC disciplined to PPS; RTC periodically corrected/trimmed.
* **`GPS_DEGRADED`**: PPS present but GPS labeling degraded (no fix or invalid UTC); PHC can still be disciplined in frequency (if you trust PPS), but UTC mapping is suspect.
* **`HOLDOVER_RTC`**: GPS lost; PHC runs on holdover discipline strategy; RTC becomes authoritative for “wall clock” continuity.
* **`RECOVERY`**: GPS returns; re-associate PPS↔UTC, step/slew decisions, re-sync RTC and re-enter lock.

### Control principles

* **PHC discipline**:

  * In `GPS_LOCKED`: use PPS as the primary reference (phase/frequency).
  * In `HOLDOVER_RTC`: do *not* chase RTC every second; instead, use a slow steering policy (PHC free-runs with last freq + optional very low-gain correction from RTC) to avoid injecting RTC noise into PHC.

* **RTC discipline**:

  * In `GPS_LOCKED`: ensure RTC time is correct (one-time set) and then use **aging offset** adjustments slowly to minimize drift.
  * In `HOLDOVER_RTC`: RTC becomes the fallback source for system time or for UTC estimate, but do not repeatedly “set” it unless you have a superior reference.

---

## 4) Scheduling/affinity recommendations (RPi 5 specific, aligned with what you already do)

* Pin **Thread A** to isolated core 2.
* Move USB/xHCI interrupts off core 2 (you already did).
* Also move your NIC interrupts off core 2 if you haven’t yet (important for PHC thread determinism).
* Give Thread A `SCHED_FIFO` (e.g., prio 80), keep it short.
* Give Thread B and C normal priority (or low FIFO if you want bounded latency without starving the system).

---

## 5) The key fix your trace demonstrated: GPS must never block the PPS→PHC path

With this architecture, even if `/dev/ttyACM0` blocks for ~1 second (as your `strace` shows), it only blocks **Thread B**, and **Thread A** continues to:

* take the PPS
* sample PHC immediately
* run the PHC servo loop
* feed both PTP traffic and RTC discipline

That directly targets your “HIGH LATENCY” symptom.

---

## 6) RTC discipline details (DS3231) that work well in practice

A robust DS3231 discipline loop tends to behave best if you:

1. **On initial lock / after GPS recovery**

   * Set RTC time once (step) to GPS UTC.
   * Clear drift accumulator.

2. **During steady lock**

   * Estimate RTC drift over a longer window (e.g., 5–30 minutes).
   * Adjust aging offset in small steps with hysteresis:

     * e.g., only change if estimated drift exceeds ±0.1 ppm (your current threshold seems in this spirit)
     * clamp step size (±1 or ±2 LSB per update)
     * enforce a minimum interval between changes (e.g., 10 minutes)

3. **Holdover**

   * Freeze aging offset updates (or reduce aggressiveness).
   * Keep RTC running as-is; treat it as a stable oscillator.

This prevents the RTC servo from “hunting” and avoids coupling transient GPS noise into your fallback clock.

---

## 7) Implementation priority order (practical next steps)

1. **Split threads**: create Thread A (PPS+PHC) and Thread B (GPS).
2. **Make Thread A bounded**: no serial reads, no I2C, no allocations.
3. **Add observation ring buffer** Thread A → Thread C (RTC).
4. **Add state machine** for GPS lock / holdover / recovery.
5. Only then tune servo gains and thresholds.

---

If you paste:

* your current main loop structure (high level is fine),
* how you read PPS (`time_pps_fetch` or something else),
* how you sample PHC (which ioctl / method),
* and how you currently set/adjust DS3231,

…I can propose a concrete thread split + data structures that minimally disrupts your existing codebase while ensuring PHC and RTC are both disciplined correctly.
