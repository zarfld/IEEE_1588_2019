Below is a practical, field-tested way to discipline a **DS3231 RTC** to **GPS UTC time** when you have both **NMEA** and a **1PPS** signal.

The key principle is:

* **1PPS defines the exact second boundary** (phase).
* **NMEA provides the second number/date** (time-of-day label).
* You *never* treat NMEA arrival time as “the second edge” (serial latency/jitter makes that wrong).

---

## 1) Hardware and signal integrity best practices

### PPS electrical

* Use **clean edges** into your interrupt/GPIO (short wire, good ground).
* Ensure **correct voltage levels** (many GPS modules output 3.3 V PPS; DS3231 is I²C; your host GPIO must match).
* Add a **pull-up/pull-down** as appropriate (depends on PPS output type). Avoid floating input.
* If you have long cable runs: consider **Schmitt trigger buffering**.

### DS3231 power domain

* Use a proper **backup battery** (CR2032 or rechargeable variant if the module expects it).
* Confirm **OSF (Oscillator Stop Flag)** handling: if OSF is set, treat RTC time as untrusted until re-set.

---

## 2) Time model: separate “label” from “edge”

### What goes wrong if you don’t

NMEA sentences (RMC/ZDA/etc.) arrive with:

* variable UART latency,
* buffering delays,
* OS scheduling jitter (on Linux),
* GPS module-specific output timing (often tens to hundreds of ms after the second).

So the correct approach is:

1. On each PPS edge, record a **local timestamp** (monotonic clock / hardware capture if possible).
2. Parse NMEA to obtain the **UTC time-of-day for a specific second**.
3. **Associate** the NMEA-reported second with the **nearest PPS edge** (typically “the last PPS before this sentence completed”, depending on module behavior).

---

## 3) A robust disciplining strategy (recommended)

### A) Discipline the host clock to GPS (if you have a host OS)

If you have a Linux host (e.g., Raspberry Pi), the most stable architecture is:

* GPSD (or direct driver) provides:

  * NMEA time → “time-of-day”
  * PPS → “exact second”
* Chrony/ntpd disciplines the **system clock** using PPS (and NMEA as label).
* Then the system periodically **writes the disciplined system time into the DS3231**.

This avoids trying to make the DS3231 the primary servo (it’s a *holdover* clock, not a steering-grade clock).

### B) Use the DS3231 as holdover and *slowly* calibrate it

The DS3231 has an **aging offset register** (frequency trim). Best practice:

* Only change the aging offset **slowly** (minutes/hours timescale).
* Use averaged drift estimates (not instant phase error).

---

## 4) How to set DS3231 time accurately with PPS

The DS3231 keeps time internally; you can write its seconds/minutes/hours/date registers, but “writing at a random time” gives an unknown sub-second phase error.

Best practice:

1. Wait for a PPS edge.
2. Immediately write DS3231 time registers to the **next second** (or the current second depending on your GPS module’s PPS definition).
3. Verify on subsequent PPS edges whether DS3231 second increments align as expected.

**Practical rule:**

* If PPS indicates the **start** of a UTC second `T`, then right after the PPS edge you want DS3231 to read `T` (or `T+1` if your code path can’t complete quickly). Choose one convention and stick to it.

On non-real-time systems, it is often easier to:

* Use PPS to *schedule* the write
* Accept a small residual offset (few ms) and correct with **occasional stepping** if it grows large.

---

## 5) Control loop design: step vs slew vs trim

You have three “actuators”:

### 1) Step the DS3231 time (coarse correction)

Use when:

* startup after OSF,
* GPS reacquired after long outage,
* offset is large.

Typical threshold: **> 0.5 s** (or even > 0.2 s if you want tight alignment).

### 2) Do nothing (hold)

Use when:

* GPS is unstable,
* NMEA time validity uncertain,
* leap-second/UTC-valid flags not asserted.

### 3) Adjust DS3231 aging offset (fine frequency trim)

Use when:

* you want RTC holdover drift minimized during GPS loss,
* you have enough observation time to estimate DS3231 drift.

**Important:** aging offset changes are discrete and small; treat it like a **very low-bandwidth** servo.

---

## 6) Recommended measurement method for DS3231 drift

You want to estimate how fast the DS3231 runs relative to true UTC.

### Option A (simple, good): compare DS3231 time to disciplined system time

If your system time is PPS-disciplined:

* every N minutes, read DS3231, compare to system UTC (integer seconds, plus fractional part from system).
* build a drift estimate over hours/days.

### Option B (better): measure DS3231 1 Hz SQW against PPS

If you can route **DS3231 SQW (1 Hz)** into a capture pin:

* measure phase difference between **GPS PPS** and **DS3231 SQW** edges.
* this gives you direct phase/drift without relying on I²C read timing.

This is the closest you’ll get to a clean disciplining observable.

---

## 7) Filtering and stability rules

### NMEA validation

* Prefer sentences that carry full UTC date/time: **RMC** or **ZDA**.
* Only trust time if GPS reports a valid fix/time (module-dependent flags).
* Handle leap seconds properly:

  * Many receivers provide leap second info; otherwise, be cautious during leap events.

### Filtering

* Use **median** or **trimmed mean** over a window (e.g., 5–30 samples) for measured offsets.
* Reject outliers (UART stalls, OS jitter spikes).

### Update cadence (typical)

* **Step DS3231 time**: at startup and only if error exceeds threshold.
* **Write DS3231 from system time**: every **10–60 minutes** (depending on how much drift you tolerate).
* **Adjust aging offset**: at most every **1–6 hours**, based on long-term drift estimate.

---

## 8) Suggested servo logic (practical pseudo-code)

```text
On PPS interrupt:
  t_pps = monotonic_timestamp()
  mark "new second edge"

On NMEA receive (RMC/ZDA with valid UTC):
  utc_label = parsed UTC second (integer)
  associate utc_label with nearest PPS edge (usually the last PPS)
  now you have: PPS_edge_time corresponds to utc_label

Periodically (e.g., every 30–60 s):
  rtc = read_DS3231()
  err = (rtc_time - utc_label_time_at_recent_pps)

  if OSF set OR abs(err) > 0.5 s:
      schedule DS3231 set at next PPS (step)
  else:
      accumulate err over long window to estimate drift (ppm)
      if enough data (hours):
          compute desired aging_offset change (small step)
          apply aging offset rarely (hours), with bounds and hysteresis
```

Key points:

* The **phase reference is PPS**, not “time you read NMEA”.
* Treat **aging offset** as slow calibration, not as a per-second controller.

---

## 9) If you are on Linux (Raspberry Pi) — operational best practice

High-level flow:

1. Use PPS GPIO → kernel PPS subsystem.
2. Use NMEA via gpsd (or direct).
3. Chrony disciplines system time using PPS.
4. Periodically sync DS3231 from system time.

Operationally, this yields:

* tight UTC on the system clock (sub-millisecond if configured well),
* DS3231 kept close (usually within tens of ms to <1 s depending on policy),
* good holdover when GPS drops.

---

## 10) Common failure modes to avoid

* **Using NMEA arrival time as UTC** (guaranteed jitter/offset).
* **Updating DS3231 too often** (creates noise; I²C timing jitter looks like drift).
* **Aggressive aging offset changes** (causes limit cycles).
* **Ignoring OSF** after power events.
* **Not validating GPS time** (invalid fix/time can “discipline” you wrong).

---

## Quick decision table

| Situation                          | What to do                                              |
| ---------------------------------- | ------------------------------------------------------- |
| First boot, OSF set                | Step DS3231 from GPS at PPS boundary                    |
| GPS stable, system PPS-disciplined | Occasionally write DS3231 from system time              |
| Need better holdover               | Estimate drift over hours, adjust aging offset slowly   |
| GPS lost                           | Use DS3231 as holdover; avoid frequent steps; log drift |
| Offset suddenly large              | Step once; do not “chase” with trim                     |

---

If you tell me your platform (bare-metal MCU vs Raspberry Pi/Linux), your GPS module (u-blox? Quectel? etc.), and whether you can route **DS3231 SQW** into an input capture pin, I can provide a concrete configuration (chrony/gpsd approach on Linux or an embedded PI controller with realistic thresholds and aging-offset step sizing).
