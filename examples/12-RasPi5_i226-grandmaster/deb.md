You now have hard proof of *what is wrong*:

1. **You are detecting the ±1 s condition correctly**, but
2. **you are still feeding it into the drift estimator**, and
3. the “GPS time” you compare against for RTC drift is **not the correct reference for a 1-Hz RTC** (you are effectively comparing an *integer RTC second* to a *near-end-of-second* “GPS time”, hence the repeated `-997 ms`).

That combination guarantees poisoning.

---

## 1) Stop poisoning: discontinuity must RESET + SKIP, not “add outlier”

Your debug literally says:

```
USING: -997.615 ms (abs > 100ms!)
Expert prediction: This will contaminate drift average!
[DEBUG Ring Buffer] OUTLIER drift added to buffer!
```

That is the bug. The correct behavior is:

* if `abs(err_ms) > threshold` (100 ms is fine):

  * **reset ring buffer**
  * **reset last_error / last_valid**
  * **do not compute drift_ppm**
  * **do not push anything**
  * optionally **step RTC** (if you decide it is truly off by ≥1 s)
  * return

### Implementation pattern (drop-in logic)

In your RTC drift update function:

```cpp
const double err_ms = (double)err_ns / 1e6;

if (std::abs(err_ms) > 100.0) {
    rtc_drift_filter.reset();
    rtc_last_error_valid = false;   // critical: prevents err_delta using old value
    // optionally: log + maybe force a one-time RTC set
    return;                         // critical: do not push to buffer
}

if (!rtc_last_error_valid) {
    rtc_last_error_ns = err_ns;
    rtc_last_error_valid = true;
    return;                         // first valid sample establishes baseline
}

const int64_t delta_ns = err_ns - rtc_last_error_ns;
const double drift_ppm = (double)delta_ns / 1e3; // (ns/sec)/1000 = ppm

rtc_last_error_ns = err_ns;
rtc_drift_filter.push(drift_ppm);
```

Do the *same* for PHC drift.

This alone will eliminate the persistent `16667 ppm` averages, because they will never get created.

---

## 2) Your RTC error of ~ -997 ms is not “drift” – it’s a reference mismatch

Look at the first discontinuity dump:

```
RTC time: 1768204460.0
GPS time: 1768204460.997615050
Error vs GPS: -997.615 ms
```

That is exactly what happens if:

* **RTC read is integer seconds** (e.g., `...4460.0`)
* your “GPS time” includes a fractional part that is **near the end of the second** (`...4460.9976`)

So even when the RTC second is correct, you can manufacture a ~-997 ms “error” simply by comparing to the wrong fractional reference.

### Correct reference for disciplining a 1 Hz RTC (DS3231)

For DS3231 you should discipline against an **integer second boundary** derived from your PPS↔UTC mapping, not against “NMEA arrival time” or any time-with-fraction unless you explicitly model the read latency.

Concretely:

* After `[PPS-UTC Lock]` you already compute:

```
base_pps_seq
base_utc_sec
```

From that, the expected UTC second at PPS `seq` is:

```cpp
expected_utc_sec = base_utc_sec + (seq - base_pps_seq);
```

That is the correct reference for a seconds-resolution RTC.

### Practical rule for comparing RTC to UTC at PPS

When you sample RTC shortly *after* PPS, it might still read the same second or the next second depending on read latency. So compare against both and choose the closest:

```cpp
int64_t rtc_sec = read_rtc_seconds();
int64_t exp_sec = expected_utc_sec;

// candidates: exp_sec and exp_sec+1 (or also exp_sec-1 if you sample before PPS)
int64_t e0 = rtc_sec - exp_sec;
int64_t e1 = rtc_sec - (exp_sec + 1);

int64_t chosen_err_sec = (std::llabs(e0) <= std::llabs(e1)) ? e0 : e1;
```

Then:

* if `abs(chosen_err_sec) >= 1` → RTC is off by seconds → step it
* else → treat as aligned (no “-997 ms” nonsense exists in this domain)

If you want sub-second diagnostics, you can track *read latency* separately, but you must not interpret it as RTC phase error.

---

## 3) Why you saw repeated “-997 ms” every second

Because your code is repeatedly constructing an error in the wrong domain (seconds RTC vs fractional GPS). Once you do that once, your drift estimator then alternates between:

* `-997 ms` (wrong-domain error)
* `+2.4 ms` (after some set / different sampling)
* which yields `±1e9 ns` deltas → `±1e6 ppm`

That is exactly what your outlier logs show.

---

## 4) Concrete action list

### A. Fix the estimator behavior

* **Discontinuity → reset+skip** (do not push outliers)
* **After reset → require one baseline sample** before computing drift

### B. Fix the RTC reference

* For DS3231 drift/offset, compute error using **expected_utc_sec_at_pps(seq)**, not “GPS time with fraction”.

### C. Keep the threads separated (your preference)

* RT thread: PPS wait + PHC sample (minimal work)
* Worker thread: GPS parsing + mapping lock + RTC discipline + logging
* RTC discipline should be triggered by PPS events but should **use seconds-domain logic**.

---

## 5) Minor compile warnings you showed (quick cleanup)

* `last_nmea_time_` should be `time_t` (or cast explicitly) to silence signedness warning.
* `nmea_arrival_mono_ms` is unused; either use it in the lock logic or remove it.

---

If you paste the two specific code blocks where you compute:

1. `err_ns` for RTC vs GPS, and
2. the ring-buffer drift update (`push`, `avg`, `count`),

I can provide an exact patch with minimal diffs (C++14, no new dependencies) that implements:

* correct discontinuity handling,
* correct seconds-domain RTC discipline,
* and a clean “baseline sample required” state.
