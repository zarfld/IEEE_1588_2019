
# Spezifikation: GPS-Disciplined PTP Grandmaster (PHC + PPS + NMEA + optional RTC)

## 1. Ziel / Scope

Dieses System soll als **PTP Grandmaster** arbeiten, dessen **PHC (PTP Hardware Clock)** durch **GPS** stabilisiert wird.

**Primary objective**

* PHC muss langfristig **frequenzstabil** sein (geringer Drift), damit PTP-Clients stabil synchronisieren.

**Secondary objectives**

* Schneller “startup convergence” (nach Boot innerhalb kurzer Zeit “brauchbar”)
* Kein “1-Sekunden-Slip” durch fehlerhafte PPS↔UTC Zuordnung
* Robust gegen GPS-Dropouts, NMEA-Jitter, PPS-Missing pulses
* Keine aggressiven Steps im Regelbetrieb (Clients sollen nicht “springende Zeit” sehen)

Nicht im Scope (optional später):

* vollständiger Kalman Filter
* Multi-GNSS advanced timing holdover
* network-based servo to external GM (dieser Host IST GM)

---

## 2. Begriffe / Signale

### 2.1 Zeitquellen

* **GPS NMEA**: liefert UTC-Zeit (Sekundenauflösung + sub-second aus Parsing/Timing)
* **GPS PPS**: liefert präzises Sekunden-Tick (ns-genau), aber **ohne** absolute UTC-Sekunde
* **PHC** (`/dev/ptpX`): zu stabilisierende Hardwareclock
* **RTC** (`/dev/rtc0`) optional: DS3231 als Backup/Holdover + 1Hz SQW → `/dev/pps1`

### 2.2 Offset / Drift

* **offset(t) = PHC_time(t) − GPS_UTC_time(t)** in Nanosekunden
* **drift ≈ d(offset)/dt**

  * in ns/s, ppm oder ppb
  * 1 ppm = 1 µs/s ≈ 1,000,000 ns/s
  * 1 ppb = 1 ns/s

---

## 3. High-Level Strategie (3-Phasen-Servo)

Das System arbeitet mit **separater Offset- und Driftbehandlung**, um Stabilität zu maximieren.

### Phase A — Offset Correction (Phase Align)

Ziel: PHC **nah** an GPS bringen, um später saubere Driftmessung zu ermöglichen.

* Offset wird über mehrere Messungen geglättet (Noise Filter)
* Wenn |offset| größer als Step-Schwelle ist, wird **ein Step** durchgeführt
* Danach **Reset** der Drift-Baseline (weil Step = Discontinuity)

### Phase B — Drift Baseline (Frequency Bias Capture)

Ziel: eine stabile Baseline der Drift (Frequenzabweichung) bestimmen.

* Ein messbares Zeitfenster wird benutzt, z.B. **20 PPS pulses** (≈20s)
* Drift wird aus PHC-Delta / Ref-Delta bestimmt
* Baseline wird als initiale `freq_ppb_bias` gespeichert

### Phase C — Drift Evaluation (Run Servo)

Ziel: laufend Frequenz korrigieren, ohne häufige Steps.

* Pro Loop (typisch 1 Hz oder 2 Hz):

  * Offset messen
  * Drift aus Δoffset/Δt berechnen
  * Drift glätten (EMA oder Fenster-Mittel)
  * `clock_adjtime(ADJ_FREQUENCY)` anwenden (slew)
* Offset-Steps sind nur erlaubt, wenn sehr groß oder Lock verloren.

---

## 4. Architektur-Komponenten

### 4.1 Sensoren / Adapter

* `GpsAdapter`

  * NMEA Parser (RMC/GGA)
  * Fix-State (Quality, Satellites)
* `PpsAdapter` (`/dev/pps0` bzw `/dev/pps1`)

  * ioctl `PPS_FETCH`
  * liefert `assert_timestamp` + `assert_sequence`
* `PhcAdapter` (`/dev/ptp0`)

  * `clock_gettime` auf PHC
  * `clock_settime` (Step)
  * `clock_adjtime` Frequency adjust (ppb)
* `RtcAdapter` optional

  * ds3231 time set
  * SQW enable 1Hz + PPS1 device ready

### 4.2 Control Engines

* `PpsUtcMapper` (Critical!)

  * ordnet PPS Pulse Sequenznummer einer UTC-Sekunde zu
  * muss “freeze” nach Lock, bis Lock verloren wird
* `PhcCalibrator` (optional startup phase)

  * misst initialen freq drift über N pulses
* `ClockServo`

  * drift estimator
  * EMA Filter
  * apply adjfreq
* `GrandmasterController`

  * Orchestrator / State machine
  * entscheidet Step vs Slew vs Holdover

---

## 5. PPS↔UTC Lock & Mapping (Must-have)

### 5.1 Problem

PPS ist exakt, aber sagt nicht welche UTC-Sekunde das ist.
NMEA sagt UTC, aber ist jitterig / delayed (~100–500ms).
Wenn PPS falsch einem UTC-Sekundenwert zugeordnet wird → **±1s slip**.

### 5.2 Requirement: “Association Lock”

Lock gilt als erreicht, wenn mehrere NMEA-Zeitstempel konsistent den **gleichen** “zuletzt geschehenen PPS” referenzieren.

**Lock Condition (example)**

* über die letzten `K=5` Sekunden:

  * die NMEA-Zeitsekunde `utc_sec` steigt monoton +1
  * NMEA arrival latency liegt in Range `[min_latency, max_latency]` (z.B. 20–800ms)
  * keine PPS Dropouts im selben Zeitraum
    → dann gilt: “NMEA labels LAST PPS”

### 5.3 Freeze Rule

Sobald Lock erreicht ist:

* setze `base_pps_seq = current_pps_seq`
* setze `base_utc_sec = utc_sec` (aus NMEA)
* ab jetzt gilt permanent:

[
utc_sec(pps_seq) = base_utc_sec + (pps_seq - base_pps_seq)
]

**Absolute MUST**

* Dieses Base-Mapping darf sich **nicht ändern**, solange Lock nicht verloren wird.

### 5.4 Lock Loss Conditions

Lock wird verworfen wenn:

* PPS dropout (seq delta > 1) über Threshold
* NMEA Fix verloren / status != A
* NMEA time jump rückwärts/zu weit vorwärts
* jitter/latency völlig außerhalb plausibler Grenzen
  Dann:
* Mapping invalid
* Servo geht in Holdover oder Reacquire

---

## 6. Offset Measurement (synchron & stabil)

### 6.1 Messdefinition

Ein Offset-Sample besteht aus:

* `pps_seq` = letzte PPS sequence
* `gps_utc_ns` = **UTC Zeitpunkt dieses PPS** (aus Mapping!)
* `phc_ns` = PHC timestamp **zum exakt selben PPS Ereignis**

**Best practice:** PHC wird *auf die PPS-Flanke* getimestamped:

* wenn PHC PPS Input unterstützt: preferred
* ansonsten: read PHC unmittelbar nach PPS-FETCH und korrigiere softwareseitig (schwächer, aber ok)

### 6.2 Offset Sample Quality Gate

Ein Offset Sample ist **gültig**, wenn:

* Mapping locked
* PPS dropout == 0 in dieser Sekunde
* `phc_ns` gelesen innerhalb X ms nach PPS event (z.B. <10ms)
* keine PHC step occurred seit Baseline start

---

## 7. Phase A: Initial Offset Correction

### 7.1 Noise Filtering für Offset

Offset Noise wird geglättet über:

* Median of N (empfohlen N=5)
  oder
* EMA mit α≈0.2

### 7.2 Step Policy

Ein Step darf ausgeführt werden wenn:

* Mapping locked **ODER** (startup emergency) GPS time reliable enough
* |offset| > `STEP_THRESHOLD_NS`

Empfohlene Schwellen:

* Startup: 100ms
* Normal run: 1s (oder Step komplett disabled)

### 7.3 Step Execution

Aktion:

* `phc_step_to(gps_utc_time_now)`
* danach:

  * `ResetDriftBaseline()`
  * `SkipSamplesFor = 2..3 PPS` (damit nach Step keine falsche Drift entsteht)

---

## 8. Phase B: Drift Baseline (window measurement)

Diese Phase macht aus “Offset ist jetzt ungefähr 0” eine “PHC läuft auch *richtig*”.

### 8.1 Baseline Measurement Window

Parameter:

* `N_PULSES = 20` (typisch 20…60)
* Messung basiert auf 2 PHC-Timestamps:

  * `phc_start_ns` bei PPS seq S
  * `phc_end_ns` bei PPS seq S+N

Referenz delta:

* `ref_delta_ns = N_PULSES * 1e9`

PHC delta:

* `phc_delta_ns = phc_end_ns - phc_start_ns`

### 8.2 Drift Computation

[
ratio = \frac{phc_delta}{ref_delta}
]
[
drift_{ppm} = (ratio - 1.0) \times 10^6
]
[
drift_{ppb} = (ratio - 1.0) \times 10^9
]

### 8.3 Validity Checks

Baseline measurement ist gültig wenn:

* keine PPS Dropouts im Fenster
* keine Steps im Fenster
* drift_ppm innerhalb plausibler Grenze

Empfohlene Grenzen:

* Hard: ±2000 ppm (alles darüber = völlig falsch)
* Soft: ±200 ppm (realistisch für brauchbare Uhr)

### 8.4 Application

Wenn gültig:

* `freq_bias_ppb = -drift_ppb`
* `phc_adjfreq(freq_bias_ppb)`
* log: baseline drift, applied bias

Wenn ungültig:

* baseline reset, retry max R times, sonst timeout/fallback

---

## 9. Phase C: Drift Evaluation (laufender Servo)

### 9.1 Loop Rate

Empfohlen: **1 Hz** (jede PPS Sekunde ein Sample)

### 9.2 Drift aus Offset-Gradient

Pro gültigem Sample:

* `offset_ns[n]`
* `offset_ns[n-1]`
* `Δt = 1s` (oder tatsächliche Zeit zwischen seqs)

Drift estimate:
[
df_{ppb} = \frac{offset[n] - offset[n-1]}{\Delta t_s}
]
Weil ns/s ≡ ppb

Beispiel:

* offset steigt um +50ns pro Sekunde ⇒ +50 ppb Drift

### 9.3 Filter

Drift ist noisy → filter:

EMA:
[
df_{ema} = \alpha \cdot df + (1-\alpha)\cdot df_{ema}
]

Empfohlen:

* α = 0.05 … 0.2 (je nach Jitter)
* α klein = ruhig, reagiert langsamer

Option: Zusätzlich clamp:

* `|df| <= DF_CLAMP_PPB` (z.B. 500000 ppb = 500 ppm)

### 9.4 Frequency Command

Cumulative frequency:

[
freq_{total} = freq_{bias} + df_{ema} + freq_{pi}
]

Minimal-Version ohne PI:

* `freq_total = freq_bias + df_ema`

Optional PI (small correction to fight residual offset):

* `freq_pi += k_i * offset_ns`
* `k_i` sehr klein (z.B. 0.001 ppb/ns)
* aber nur wenn ihr es wirklich braucht

### 9.5 Apply Rule

* apply `phc_adjfreq(freq_total)`
* rate-limit:

  * max change per second: `MAX_FREQ_STEP_PPB` (z.B. 20000 ppb)

### 9.6 Offset Safety

Wenn Offset wächst trotz Servo:

* wenn |offset| > `ALARM_OFFSET_NS` (z.B. 10ms):

  * warn
  * increase aggressiveness (α höher / allow bigger freq change)
* wenn |offset| > `EMERGENCY_STEP_NS` (z.B. 500ms oder 1s):

  * step allowed, aber:

    * reset baseline
    * skip samples

---

## 10. “Do not disturb measurement window” Regel (deine Kernforderung)

Für Window-basierte Driftmessung gilt:

**Wenn Driftmessung über Δt Fenster aktiv ist:**

* Step ist verboten
* freq_adjust wird entweder:

  * eingefroren, oder
  * nur “sehr langsam” verändert (empfohlen: einfrieren)

**Warum:** jede Veränderung während des Fensters verfälscht Δoffset/Δt.

Implementation Rules:

* `servo_mode = MEASURE_DRIFT_WINDOW`
* in diesem Mode:

  * `adjfreq` nicht verändern
  * nur Sample sammeln

Nach Fenster:

* Drift berechnen
* dann `adjfreq` setzen

---

## 11. Holdover / Degraded Modes

Wenn GPS Lock verloren:

### 11.1 Short Holdover (seconds…minutes)

* Servo stoppt Offset-Gradient measurement (kein valid offset)
* PHC bleibt auf letzter `freq_total`
* optional: RTC PPS1 als Ersatz-Reference (wenn DS3231 SQW gut)

### 11.2 Recovery (Reacquire)

* mapping reacquire
* offset neu messen
* wenn offset groß → Step
* baseline drift neu bestimmen
* dann normal run

---

## 12. Logging & Diagnostics (Pflicht für Debugging)

### 12.1 Pflichtfelder pro Loop

* `pps_seq`, dropout flag
* `gps_utc_ns` (für PPS)
* `phc_ns` (zum PPS)
* `offset_ns`
* `df_ppb_raw`, `df_ppb_ema`
* `freq_bias_ppb`, `freq_total_ppb`
* lock state (mapping locked? gps fix? holdover?)

### 12.2 Pflichtlogs bei Events

* “Mapping locked/unlocked”
* “Step applied”
* “Baseline drift measured / rejected”
* “Calibration timeout”
* “PPS dropout detected”
* “Large offset alarm / emergency step”

---

## 13. Parameter (Defaults)

**Mapping**

* `LOCK_K = 5`
* `NMEA_LATENCY_MIN_MS = 20`
* `NMEA_LATENCY_MAX_MS = 800`

**Offset step**

* `STEP_THRESHOLD_NS_STARTUP = 100ms = 100,000,000ns`
* `STEP_THRESHOLD_NS_RUN = 1s = 1,000,000,000ns`
* `EMERGENCY_STEP_NS = 500ms…1s`

**Baseline drift**

* `N_PULSES_BASELINE = 20`
* `DRIFT_THRESHOLD_PPM = 2000` (hard reject)
* `DRIFT_SOFT_PPM = 200` (warn)

**Servo**

* loop rate: 1 Hz
* `EMA_ALPHA = 0.1`
* `MAX_FREQ_STEP_PPB = 20000`
* `DF_CLAMP_PPB = 500000` (500ppm)

---

## 14. Acceptance Criteria / Tests

### 14.1 Unit Tests

* PPS sequence tracking: dropout detection delta logic
* mapping freeze: base mapping unchanged after lock
* drift calculation correctness: known synthetic offsets → expected drift
* EMA filter behavior

### 14.2 Integration Tests (real hardware)

1. Startup convergence:

* nach Step + baseline drift:

  * offset < 5ms nach 30s

2. Steady state:

* offset RMS < 1µs (je nach hardware)
* df_ema stabil

3. GPS dropout:

* holdover keeps monotonic time
* reacquire recovers without repeated steps

4. “No 1s slip”:

* mapping must not re-anchor while locked

---

# Kurze Umsetzungshilfe (State Machine)

Empfohlene Zustände:

1. `WAIT_GPS_FIX`
2. `WAIT_PPS_LOCK` (Mapping lock)
3. `INITIAL_STEP_IF_NEEDED`
4. `BASELINE_DRIFT_CALIBRATION`
5. `RUN_SERVO`
6. `HOLDOVER`
7. `REACQUIRE`

Transitions:

* Lock lost → HOLDOVER
* Lock regained → REACQUIRE → ggf. STEP → BASELINE → RUN

---

# Wichtigster “Best Practice” Satz (damit ihr nicht wieder hängen bleibt)

✅ **Offset Step ist selten**
✅ **Freq adjust ist ständig**
✅ **Drift ist Δoffset/Δt (nicht offset selbst)**
✅ **Mapping (pps_seq ↔ utc_sec) muss nach Lock eingefroren sein**
✅ **Step/Discontinuity → Drift Baseline reset + sample skip**
