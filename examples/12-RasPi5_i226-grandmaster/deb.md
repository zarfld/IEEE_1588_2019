Your latest run confirms two things:

1. **The build is clean** (link succeeds, executable rebuilt).
2. The **PHC calibration window is now effectively 20 seconds** (20 PPS pulses).

You can see this directly in the log:

* `Will measure over 20 pulses...`
* `Progress: 5/20` at PPS `#37014`
* `Progress: 10/20` at PPS `#37019`
* `Progress: 15/20` at PPS `#37024`
* `Progress: 20/20` at PPS `#37029`

That is a **20-pulse span**, i.e. **20 seconds** at 1PPS.

---

## However: the measured PHC “drift” is still pathological

Example (first completed iteration):

```
Measured -56920.5 ppm drift
PHC delta: 18861590790 ns, Ref delta: 20000000000 ns
```

That ratio is:

* 18.861590790 s / 20.000000000 s ≈ **0.9430795395**
* which corresponds to roughly **-56,920 ppm** (≈ **-5.7%**)

An i226 PHC should not be off by **percent-level**. Normal PHC frequency error is typically **single-digit ppm** (or at worst tens of ppm), not **tens of thousands of ppm**.

### Why your correction never converges

You also clamp/limit correction:

```
Current total: 0 ppb, Correction: 500000 ppb, New total: 500000 ppb
```

* `500000 ppb` = **500 ppm**
* but you “need” on the order of **~57,000 ppm** (≈ **57,000,000 ppb**) if that drift were real

So the loop saturates and will keep printing ~-60k ppm forever.

---

## Most likely root causes (ranked)

### A) You are not measuring “PHC at the PPS edge” consistently

If your “PHC sample” is taken **not tightly bound to the PPS event**, the measured delta can be wrong by a large fraction of a second over a 20 s window (phase wander / blocking delays / scheduling).
That can easily produce a bogus “frequency error”.

**What to do**

* Ensure the PHC timestamp used for calibration is captured **immediately after each PPS fetch**, and preferably using a method that provides a **coherent snapshot**.

Best practice on Linux is to use one of:

* `PTP_SYS_OFFSET_PRECISE` (or `PTP_SYS_OFFSET_EXTENDED`) to get near-simultaneous system + PHC timestamps, or
* a dedicated “sample PHC right after PPS edge” loop with strict error checks and rejection of late samples.

At minimum, log the **latency** between PPS event and PHC read (in ns). If you sometimes read PHC hundreds of ms late, your frequency estimate becomes meaningless.

---

### B) Wrong PHC device (or mismapped interface → /dev/ptpX)

Even though you print:

```
Interface: eth1
PHC: /dev/ptp0
```

you should **prove** that `/dev/ptp0` is the PHC for `eth1` at runtime (don’t assume).

**Verify with sysfs**

```bash
readlink -f /sys/class/net/eth1/ptp
ls -l /dev/ptp*
cat /sys/class/ptp/ptp0/clock_name
ethtool -T eth1
```

If that link doesn’t point to `ptp0`, you are calibrating the wrong clock.

**Hardening suggestion (code)**
Resolve the PHC device from `/sys/class/net/<if>/ptp` and open that, rather than hardcoding or taking “first PHC found”.

---

### C) PHC read path / clockid conversion bug

If you construct the `clockid_t` incorrectly (e.g., `FD_TO_CLOCKID(fd)` misuse, fd lifetime issues, wrong fd), `clock_gettime()` can silently give you the *wrong* clock (or you ignore errors).

**Hardening suggestion**

* Check return codes from every `clock_gettime(phc_clkid, …)` and print `errno` on failure.
* Log a one-time sanity check: read PHC twice with `sleep(20)` and verify delta ~ 20 s.

---

## Quick sanity tests you can run immediately

### 1) Does PHC advance ~1 second per second?

```bash
sudo phc_ctl /dev/ptp0 get
sleep 20
sudo phc_ctl /dev/ptp0 get
```

The difference should be ~20 seconds (plus/minus tiny).
If you see ~18.8 seconds consistently here, you have a **kernel/driver/clock selection** issue. If it is ~20 s, your **calibration sampling logic** is the culprit.

### 2) Confirm eth1 ↔ PHC mapping

```bash
readlink -f /sys/class/net/eth1/ptp
```

This should end in `/sys/class/ptp/ptp0` if your printed PHC is correct.

---

## Recommended guardrails in the code (to prevent “bad calibration”)

1. **Reject unrealistic drift estimates** before applying correction. Example:

* if `abs(drift_ppm) > 2000` → treat as invalid sample (log and skip)

  * 2000 ppm is already extremely generous; real PHC should be far below that.

2. **Require PPS-UTC lock** before starting baseline and measurement.
   You currently start calibration very early; if the system isn’t properly locked/mapped yet, you can end up mixing inconsistent epochs/phases.

3. **Measure and log sample latency** (PPS timestamp vs time you read PHC). If latency isn’t bounded tightly, the estimate is not trustworthy.

---

If you paste the relevant calibration block (the part that captures PPS, reads PHC, computes `PHC delta`), I can point to the exact line where the sampling is likely decoupled from the PPS edge and propose a deterministic sampling strategy (ideally `PTP_SYS_OFFSET_PRECISE` based) that removes scheduler-phase error from the calibration.
