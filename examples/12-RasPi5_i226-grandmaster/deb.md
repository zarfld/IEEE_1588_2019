## 1) “Do we now only get positive (or only negative) deviations because of the blocking call?”

### The key distinction

* **`offset_from_edge` will always be ≥ 0 within the current second** because you compute it as `now_utc_ns - edge_utc_ns`, and `now` is sampled *after* the PPS edge was timestamped. So that particular number is inherently one-sided.
* **Drift (ppm) is not derived from that value directly.** Drift comes from **Δ(error) over Δ(time)**. That **can be positive or negative** depending on whether the RTC second boundary is running fast or slow versus GPS.

### What your new log shows

Your drift measurements are clearly **both signs**, and the running average converges near zero, which is what you want if the oscillator is stable:

* Example of **negative drift**: `drift=-0.583ppm` 
* Example of **positive drift**: `drift=1.124ppm` 
* Example of longer run: `Avg(60): 0.012 ppm`  and `Avg(60): 0.026 ppm` 

So: **no, the blocking PPS fetch does not force drift to be only positive or only negative**, provided drift is computed from **successive error differences** (as your log indicates).

### The *real* risk with blocking + missed edges

If the RT thread is delayed enough, you can **miss one or more PPS events**. Then:

* `seq` jumps (you see gaps in sequence numbers).
* If your drift logic assumes a fixed interval (e.g., “10 s”) while you actually observed **11 s or 12 s worth of PPS steps**, the ppm estimate can be biased.

**Recommendation (robustness):**

* Use `seq_delta = seq_now - seq_prev` and set `Δt = seq_delta * 1s` (or, for your 10-second cadence, `seq_delta * 1s` accordingly).
* If `seq_delta != expected`, either:

  * compute ppm using the true `Δt`, or
  * discard that sample (treat as dropout).

That addresses the only systematic drift-estimation failure mode introduced by missing edges.

### One bug you should fix (visible in the log)

In one run, the drift debug output shows an obviously broken value:

`Avg(2): -0.085 ppm (9223372036854774956ns/10s)` 

That looks like a **signed/unsigned formatting or overflow** artifact (very likely printing a negative `int64_t` as `uint64_t`, or mixing `%ld/%lld` with the wrong type). Fix that first, otherwise you may misread the filtering behaviour even when the underlying math is fine.

---

## 2) “What about PHC adjustment/disciplining — is that fine? Should we use a floating average?”

### What looks good in your PHC flow

You:

* establish a **PHC calibration baseline** at a PPS edge and then
* measure drift over **20 pulses** and report a ppm result (e.g. `Final drift: -23.5 ppm` or `9.2 ppm`).

That’s a valid calibration/characterisation step.

### What calibration is **not**

That 20-pulse calibration alone is not a complete disciplining loop. A disciplining loop needs:

* a **continuous phase error measurement** (PHC time vs PPS),
* a **servo** (PI/PID or equivalent) that drives:

  * **frequency** (via `clock_adjtime` / `ADJ_FREQUENCY`), and optionally
  * **phase** corrections (carefully, to avoid steps once locked).

### Should you use a floating average?

Yes — but with discipline:

**Use averages to estimate frequency, not to hide phase steps.**

A practical pattern:

1. **Phase error per second**: `e[n] = (phc_at_pps[n] - gps_pps_utc[n])` (in ns)
2. **Frequency estimate** (per second): `df[n] = e[n] - e[n-1]` (ns/s ≈ ppb)
3. Filters:

   * **Short-window median** (or outlier clamp) on `e[n]` to reject occasional latency spikes.
   * **EMA (exponential moving average)** on `df[n]` to get a stable frequency estimate.
4. Servo:

   * Apply `adjfreq` from the filtered `df[n]` (frequency discipline).
   * Avoid phase steps once locked; if you must, do it only on acquisition or after GPS loss/reacquisition.

**Why this works:** you keep the loop responsive to real oscillator drift, while not letting one missed wakeup (or one delayed PPS fetch) pollute your frequency correction for minutes.

### Two concrete PHC sanity checks you should add

1. **Lock quality metrics**:

   * `max_jitter` (you already print this for GPS PPS) and a rolling percentile of phase error.
2. **Missed PPS detection**:

   * if `seq_delta != 1`, log it and freeze/hold last frequency command for that cycle.

---

## 3) One operational detail you tripped over: `adjtimex` “not found”

On Debian/RPi OS it’s commonly in `/usr/sbin`, which may not be in a normal user’s `PATH`. Use:

* `sudo adjtimex --print | grep tai`
* or `/usr/sbin/adjtimex --print | grep tai`

(Your program output claims it retrieves TAI-UTC via `adjtimex()`, which is fine internally, but you still want the CLI available for validation.)

---

If you want, paste (or point me to) the specific drift computation code section (the part that computes `ΔErr` and divides by “10s”), and I’ll tell you exactly where to incorporate `seq_delta` so missed edges cannot distort ppm.


