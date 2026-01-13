## Holdover strategy: should the servo switch to RTC (with its own PPS)?

Yes—**but only in a constrained way**.

Best practice is to treat the RTC as a **flywheel frequency/phase reference**, not as a new “absolute time-of-day authority” once GNSS is gone.

### Why this makes sense in your setup

You already have two key properties in the logs:

* The RTC is explicitly **synchronized to GPS time**, with a known discontinuity window that you already handle by skipping drift samples.
* The RTC SQW PPS (/dev/pps1) is **continuous**, and its jitter as timestamped by PPS is in the low-microsecond range in your traces (typ. ~1.5–1.7 µs from edge-to-“now” in the printouts), which is good enough for *holdover disciplining bandwidths* (slow).

So during GNSS loss, using RTC PPS as the reference can stabilize the PHC frequency versus “pure free-run,” **as long as you do not re-anchor seconds mapping in a way that can create ±1 s slips**.

---

## Recommended state-machine behavior

### State 1: `LOCKED_GPS`

**Reference:** GPS PPS + GPS time-of-day (NMEA/RMC).
**Actions:**

* Normal PHC tracking against GPS PPS.
* Keep RTC continually aligned to GPS (your RTC sync mechanism already does this and marks a discontinuity).
* Maintain/“freeze” your PPS↔UTC mapping anchors once locked (you already call out the risk of anchor changes causing ±1 s slips).

### State 2: `HOLDOVER_RTC`

**Trigger:** GPS PPS missing/unstable, or GPS ToD invalid.

**Reference:** RTC PPS (SQW) for *frequency/phase*, while **time-of-day continues from PHC** (last known good alignment).

**Actions (practical):**

* **Freeze the GPS-based mapping anchors** (do not rebuild them from RTC while GNSS is absent).
* **Freeze or heavily slow down the servo integrator** (classic holdover technique).
* Continue to apply PHC frequency trims, but with **very low bandwidth** updates:

  * Use RTC PPS edges to compute `offset_rtc = PHC_at_edge − predicted_edge_time`.
  * Update `freq_cmd` with a slow EWMA/PI (time constant minutes), not seconds.
* Add a guardrail: if RTC-PHC phase error exceeds a threshold (e.g., >100 ms), **stop using RTC for phase** and fall back to pure PHC holdover (frequency hold only). This prevents “bad RTC” from dragging you.

**What you should *not* do in HOLDOVER**

* Do **not** “re-lock” PPS↔UTC association using RTC edges. That is the path that creates the exact ±1 s failure mode you already flagged.

### State 3: `RECOVERY_GPS`

**Trigger:** GPS PPS returns and GPS ToD is valid again.

**Actions:**

* Require a **reacquisition window** (e.g., 5–30 consecutive good PPS + consistent ToD) before declaring lock.
* When you switch back:

  * **Reset servo integrator state** (or at least de-bias it).
  * **Skip N samples** after any explicit time set / mapping rebuild (you already do this pattern for RTC; do the same for PHC servo).

---

## What is “best” in practice?

A robust grandmaster typically uses this priority order:

1. **GNSS PPS + GNSS ToD** (primary truth)
2. **PHC holdover** (keep last disciplined frequency; no time steps)
3. **Optional RTC-assisted holdover** (use RTC PPS only as a slow stabilizer, only if RTC was disciplined while GNSS was good)

Given your logs show active RTC synchronization to GPS with explicit discontinuity handling, you are in the good category for step (3).

---

## One very specific implementation detail to get right

If your drift/servo math assumes fixed Δt (e.g., “10 seconds”), then during holdover you must **derive Δt from the PPS sequence delta**, because missed edges or scheduling stalls will otherwise bias the estimate. You already have PPS `seq` in your traces, so this is straightforward.

---

If you want, paste the part of your code where you (a) detect GPS loss and (b) select which PPS source drives PHC disciplining. I can propose concrete transition criteria (good/bad PPS, ToD consistency, hysteresis) that match your current logging and avoid second-slip regressions.
