You fixed **one** class of ambiguity (you now lock “NMEA labels LAST PPS”), but the log still proves you have a **second, independent off-by-one** in the way you *apply* that association to the value you call `gps_sec` (and/or how you compute `err`).

### What is now unambiguous from the new log

1. **Association lock looks correct**

   * `PPS-UTC Lock … NMEA labels LAST PPS (avg_dt=200ms)`
   * `avg_dt≈200ms` is a perfectly plausible “RMC comes ~200 ms after PPS” pattern.

2. **The ±1 second flip is still present**

   * `err=-1002.2ms` for long stretches
   * then occasionally `err=-2.2ms`
   * difference is exactly **1000 ms**.

3. **The huge drift spikes (±200000 ppm) are still artifacts**

   * `200000 ppm` corresponds to “I saw a 1 s step over ~5 s” in your estimator.
   * That’s not a DS3231 frequency problem; it’s a labeling/unwrap problem.

So: **your “LAST PPS” lock is not being used consistently downstream**, or the code still sometimes compares the RTC against `gps_sec` that is effectively **one second ahead** of “PPS second”.

---

## The most likely remaining bug (and where to look)

You likely have two time variables:

* **`gps_sec`** (derived from NMEA / RMC)
* **`pps_seq`** (from PPS device)

And somewhere you compute an “expected UTC for this PPS” as something like:

* `expected = gps_sec` sometimes
* `expected = gps_sec + 1` (or `gps_sec - 1`) other times
* or you implicitly shift because `gps_sec` is being updated on RMC arrival, not latched to a specific PPS.

Your new debug lines reinforce this suspicion:

* `[Measurement Timing] gps_sec=1768125931 pps_seq=96598`
* and later you can see the `err` toggling without a corresponding new association lock event.

That strongly suggests you are still doing a *soft coupling* (“current gps_sec”) instead of a *hard binding* (“gps_sec_for_pps_seq”).

---

## The correct, race-free model on Linux

You need a single mapping of **PPS sequence → UTC second**.

Implement it as a base pair:

* `base_pps_seq`
* `base_utc_sec`  (UTC seconds since epoch that correspond to `base_pps_seq`)

Then for **any** PPS event with sequence `s`, compute:

* `utc_at_pps(s) = base_utc_sec + (s - base_pps_seq)`

### How to update the base pair using RMC (labels LAST PPS)

When RMC arrives with `utc_rmc` and you have `last_pps_seq` at receive time:

* If you locked “labels LAST PPS”:

  * set `base_pps_seq = last_pps_seq`
  * set `base_utc_sec = utc_rmc`
* If you ever support “labels NEXT PPS”:

  * set `base_pps_seq = last_pps_seq + 1`
  * set `base_utc_sec = utc_rmc`

**Critical rule:** once this is in place, your code must **never** use a “free-running `gps_sec`” for error computation. It must use `utc_at_pps(current_seq)`.

---

## What to add to your logs (to pinpoint the exact +1)

Right where you print `err=...`, also print:

* `pps_seq`
* `utc_at_pps` (computed via base mapping)
* `rtc_sec` (DS3231 time as epoch seconds)
* `delta_sec = rtc_sec - utc_at_pps`

You will immediately see whether the persistent `-1002 ms` corresponds to:

* `delta_sec = -1` (RTC second is behind your UTC label by 1)
* or `delta_sec = 0` but your millisecond portion is being computed with the wrong sign/convention.

Given `-1002.2ms` vs `-2.2ms`, the simplest truth is: **sometimes you compare against `utc_at_pps + 1`**.

---

## RTC sync: ensure you set the RTC to the same second you later compare against

Your current line:

* `[RTC Sync] Initial sync … (error=-1002.22ms)`
* followed by “RTC synchronized”
* but then `err=-1002.2ms` continues

This indicates one of these is true:

1. **RTC set uses a different reference second** than your error computation
   (e.g., you set RTC to `utc_at_pps`, but you later compare to `utc_at_pps + 1`).

2. **RTC set is correct, but your `utc_at_pps` is wrong by +1** during measurement.

To eliminate this class entirely, do this:

* Compute `utc_target = utc_at_pps(last_pps_seq)` (or `utc_at_pps(last_pps_seq)+1` if you intentionally set to next second — but then also compare that way).
* Use **the same `utc_target` variable** for:

  * the I²C write into DS3231
  * the subsequent error computation reference.

No recomputation, no alternate path.

---

## Fix your drift estimator to be immune to ±1 s steps

Even after the labeling bug is fixed, RTC reads are quantized and can “walk” across a second boundary. Your estimator should:

1. **unwrap phase error** into a stable interval, e.g. `[-500ms, +500ms]`
2. **reject samples** where `abs(err_ms) > 200 ms` (or your tolerance)
3. compute drift only from consecutive accepted samples

This prevents garbage “200000 ppm” even if something steps once.

---

## Quick checklist (minimal changes that will stop the ±1000 ms flip)

* [ ] Replace any usage of “current `gps_sec`” in the RTC error path with `utc_at_pps(pps_seq)`.
* [ ] Maintain `base_pps_seq/base_utc_sec` updated atomically when RMC arrives.
* [ ] In the RTC set path, compute `utc_target` once and reuse it for both set and error reference.
* [ ] Add log: `pps_seq, base_pps_seq, base_utc_sec, utc_at_pps, rtc_sec, delta_sec`.

---

### If you paste the 30–60 lines around:

* where `PPS-UTC Lock` sets the association,
* where `Measurement Timing` is printed,
* and where `err=` is computed,

I can tell you exactly which variable is drifting by +1 (and whether it is a race, a sign error, or a “+1” applied in the wrong place).
