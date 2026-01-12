Your latest log is now *diagnostic-quality*: the remaining anomalies are not “general Linux jitter” anymore; they are **second-slip / discontinuity handling** and **filter contamination**.

---

## 1) The “16667 ppm” average is explainable (and fixable)

This line is the key:

```
[PPS] ... drift=1.247ppm avg=16667.630ppm(60) err=1.7ms
```

`16667 ppm ≈ 1,000,000 / 60`.

That is exactly what you get when your rolling window (60 samples) still contains **one** catastrophic outlier of roughly **±1,000,000 ppm** (i.e., a **±1 second slip** in a 1-second interval), and you compute an average over 60.

You *also* see the same contamination earlier:

```
[PPS] ... avg=-35713.233ppm(28) err=-998.3ms
```

`-35713 ppm ≈ -1,000,000 / 28`. Same phenomenon.

### What is producing the outlier?

A one-second discontinuity is already visible in your log:

* RTC side:

  ```
  [RTC Drift] Measured: -999998.716 ppm ... Error: -998.342 ms
  ```
* PHC/PPS side:

  ```
  [PPS] ... err=-998.3ms
  ```

So the system occasionally computes “this PPS is about one second off”, and then your drift estimator dutifully converts that into an absurd ppm value, poisoning the moving average until the window flushes the outlier.

---

## 2) Immediate corrective action: treat ±1s as a discontinuity, not “drift”

### A. In BOTH estimators (PHC drift and RTC drift)

Add a hard gate: if the phase error is too large, **do not feed it into the drift estimator** and **reset** the rolling window.

Recommended thresholds:

* `abs(err_ms) > 100 ms` → discontinuity (very safe)
* or `abs(err_ms) > 500 ms` → “almost certainly ±1s slip” (more permissive)

### B. Also protect calibration transitions

Right now you still emit a catastrophic PPS line immediately after calibration:

```
[PHC Calibration] ✓ Complete!
[PPS] ... err=-998.3ms
```

That is a classic “handoff” point; treat it as a discontinuity by design:

* after calibration completes: **skip 1–3 PPS cycles** (or until mapping is stable) before computing/printing drift/avg/err.

---

## 3) Why you still see intermittent “-998ms” events after lock

You already report:

```
[PPS-UTC Lock] ... LAST PPS (avg_dt=200ms)
[Base Mapping] base_pps_seq=70062 base_utc_sec=...
```

Yet later (during calibration) you still get:

```
[RTC Drift] ... Error: -998.342 ms
```

That strongly suggests **the worker thread sometimes uses a different second association** (or updates mapping inputs) even after “lock”, causing a ±1s interpretation. The lock message is not sufficient unless you enforce a policy like:

* Once locked, **freeze the mapping anchor** (`base_pps_seq`, `base_utc_sec`) and never “re-decide” LAST/NEXT on a per-message basis.
* If a newly observed NMEA/PPS delta deviates strongly, **drop that NMEA sample** as out-of-family rather than re-labeling the second.

Practical rule:

* Maintain a running mean/variance of NMEA arrival offset relative to PPS (`dt`).
* If `abs(dt - mean_dt) > 200 ms` (tune), ignore that NMEA sample for mapping/discipline.

---

## 4) Phase error of ~1.7 ms: likely not a servo issue yet, but a timestamping issue

Even once you eliminate the ±1s slips, this persists:

```
err=1.6–1.7ms
```

That magnitude is consistent with “PHC sampled *some milliseconds after* PPS edge” and then compared as if it were sampled *at* PPS.

If you want microsecond-class alignment, you need a **cross-timestamp** between system time and PHC at (or immediately around) PPS, e.g.:

* `ioctl(fd_ptp, PTP_SYS_OFFSET_PRECISE, ...)` (preferred if supported)
* or `PTP_SYS_OFFSET_EXTENDED`

Then compute **PHC time at PPS edge** by combining:

* the PPS timestamp you get from `/dev/pps0` (system clock domain)
* the system↔PHC offset from `PTP_SYS_OFFSET*_…` taken immediately after PPS

This can reduce your “phase error floor” dramatically compared to “read PHC sometime later”.

---

## 5) Minimal implementation pattern (what to change)

### A. Add “discontinuity handling” to drift update (PHC + RTC)

Conceptually:

```cpp
if (!mapping_ready || !phc_calibrated) return;

if (std::abs(err_ms) > 100.0) {
  log_second_slip(err_ms);
  drift_filter.reset();      // clears ring buffer, sums, counters
  return;                    // do not update drift/avg
}

drift_filter.push(drift_ppm);
avg = drift_filter.avg();
```

### B. Add “skip-after-calibration” guard

```cpp
if (just_finished_calibration) {
  skip_pps_updates = 2; // e.g. 2 PPS cycles
}

if (skip_pps_updates > 0) {
  skip_pps_updates--;
  return;
}
```

### C. Enforce “mapping freeze” after lock

Once you set `base_pps_seq/base_utc_sec`, do not overwrite them unless you explicitly transition to a “relock” state (GPS lost → reacquire).

---

## 6) What to expect after these fixes

1. The ±1s “-998ms” events should disappear from normal operation; if they still appear, they will be logged as discontinuities and **won’t poison averages**.
2. Your `avg` values will stay near ~1 ppm instead of swinging to ±(1e6 / window).
3. The remaining ~1.7 ms offset will become the next primary target, addressed via **PHC-at-PPS cross timestamping** (PTP_SYS_OFFSET_PRECISE/EXTENDED) and/or by taking the PHC sample deterministically as close to PPS as possible.

---

If you paste the specific drift/avg code paths (PHC and RTC) from `ptp_grandmaster.cpp` (the functions that compute `err`, `drift_ppm`, and maintain the rolling window), I can provide an exact patch (including a small, RT-safe rolling filter with a `reset()` and an outlier gate) consistent with your C++14 codebase and the current thread split.
