Your log is already diagnostic. The RTC disciplining is not “noisy”; it is **logically inconsistent by ±1 second**.

## What your log shows (key symptom)

You repeatedly alternate between:

* `err ≈ -3 ms` (good alignment)
* `err ≈ -1003 ms` (exactly “good alignment” minus ~1.000 s)

Example:

* `[RTC Sync] Initial sync … (error=-1003.07ms)`
* later: `err=-3.1ms`
* then again: `err=-1003.1ms`
* and this repeats.

That is the classic signature of an **off-by-one second labeling problem**, not DS3231 drift.

Your “drift” spikes of ±200000 ppm are a *secondary artifact*: if your error jumps by ~1.000 s over a 5 s interval, you will compute ~0.2 s/s = **200,000 ppm** (exactly what you see). In other words, the drift estimator is being fed a wrapped / mis-labeled phase error.

## Most likely root cause

You are sometimes associating **NMEA time-of-day** with the **wrong PPS edge**:

* PPS defines the *start* of a UTC second.
* NMEA sentences (RMC/GGA/VTG) arrive with variable latency and may correspond to:

  * the **last PPS** (most common), or
  * the **next PPS**, depending on module configuration and how you “latch” time in software.

If your code occasionally flips between “NMEA labels last PPS” and “NMEA labels next PPS”, your computed UTC second for a given PPS will jump by exactly 1 second, yielding the ~1000 ms error you see.

This flip often happens because of a race such as:

* “PPS interrupt updates `pps_seq`”
* “NMEA parser updates `last_utc_seconds`”
* a later computation combines `pps_seq` from one epoch with `utc_seconds` from the other (no atomic pairing / no explicit association).

## What to implement (best practice on Linux)

### 1) Make PPS the only second boundary

Maintain a monotonic “PPS tick” state:

* `last_pps_mono_ts`
* `last_pps_seq`
* `utc_sec_for_last_pps` (this is the label you care about)

On **every PPS**, you advance the timeline deterministically:

* `utc_sec_for_last_pps += 1` (once you are “locked” and have an initial label)

Do **not** re-derive “current second” from NMEA each time.

### 2) Use NMEA only to *initialize / occasionally re-anchor* the second label

When an RMC/ZDA arrives:

* parse UTC hhmmss + date → `utc_sec_candidate`
* decide whether that candidate belongs to the **last PPS** or the **next PPS**
* then set `utc_sec_for_last_pps` accordingly, but **only if it is consistent**

Practical and robust association rule (auto-detect):

* measure `dt = t_nmea_rx_mono - t_last_pps_mono`
* if `dt` is typically (for your receiver) e.g. 100–400 ms after PPS, then **RMC labels last PPS**
* if it is typically just before PPS (rare, but possible), then **RMC labels next PPS**

Lock this decision after a few samples (hysteresis), and never “flip” unless you explicitly reset.

### 3) Pair PPS and UTC label atomically

Make it impossible to combine “PPS edge A” with “UTC label from edge B”.

Concretely:

* create a single struct updated under a mutex/spinlock (or lock-free with sequence counters):

  * `{pps_seq, pps_mono_ts, utc_sec_for_that_pps}`
* When NMEA arrives, update a “pending” label, but only commit it tied to a specific `pps_seq`.

### 4) RTC set should be scheduled relative to PPS, not “now”

When you decide to step DS3231:

* do it **immediately after a PPS** and write the DS3231 to the second that PPS represents (or `+1` if your write path cannot complete fast enough—pick one convention and keep it consistent).

### 5) Fix your drift estimator: unwrap modulo 1 second

Even with correct association, any RTC read is quantized to seconds. Your phase error should be treated as:

* `phase_err_ms = (rtc - utc_label)` but mapped into a consistent interval, e.g. `[-500 ms, +500 ms]`, by adding/subtracting 1000 ms when it crosses a boundary.

And reject obvious outliers:

* if `abs(phase_err_ms) > 200 ms` (or your chosen tolerance), don’t feed it into drift.

## Quick confirmation instrumentation (minimal changes)

Add two timestamps:

1. On PPS:

* log `t_pps_mono`, `pps_seq`

2. On RMC parse completion:

* log `t_rmc_mono`, parsed `hhmmss`, and compute:

  * `dt_ms = (t_rmc_mono - t_last_pps_mono)`

If you see `dt_ms` consistently e.g. +200 ms, your receiver’s RMC labels the **last PPS**. If your code currently assumes it labels the next PPS, you will get exactly the ~1 s error pattern you’re seeing.

## Secondary checks (worth doing once)

* Ensure nothing else is writing the RTC in parallel (e.g., `hwclock` services). If another process steps `/dev/rtc1`, you can also see discontinuities.
* Confirm DS3231 OSF handling: if OSF is set, step once, clear OSF, then begin drift monitoring.

---

### Bottom line

Your DS3231 disciplining loop is failing because the **UTC second label is not consistently bound to the PPS edge**. Fix the PPS↔UTC association (and make it atomic), and the ±1000 ms toggling—and the fake ±200,000 ppm drift—will disappear.

If you paste the code section where you:

* compute `err=...` and
* decide which GPS second corresponds to the PPS,
  I can point to the exact place where the 1-second ambiguity is introduced and suggest a concrete patch structure (sequence counter / commit logic).
