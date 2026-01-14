Ja 👍 — **dein 3-Phasen-Ablauf ist “best practice”-kompatibel** und passt **sehr gut** zu dem, was in euren Plänen bereits drinsteht.

### Warum das “best practice” ist

Was du beschreibst, ist im Kern die klassische Aufteilung, die auch NTP/PTP-Servos machen:

1. **Phase (Offset) grob korrigieren**
   → “Step” oder “Phase Adjust”, damit du *nicht* minutenlang hinterherläufst.

2. **Frequenzfehler (Drift) bestimmen**
   → Drift ist **nicht** der Offset selbst, sondern die **Steigung**:
   [
   \text{drift} \approx \frac{\Delta(\text{offset})}{\Delta t}
   ]
   Damit bekommst du den *Frequenzfehler* (ppb/ppm).

3. **Frequenz nachstellen (slew / adjfreq)**
   → dann bleibt die Phase automatisch klein, ohne dauernd zu steppen.

Das ist exakt die Idee hinter dem **Frequency-Error Servo**, der in eurem Plan schon steht:
`df[n] = (phase_err[n] - phase_err[n-1]) / Δt` + **EMA Filter**. 

---

### Wichtig: Dein “wir dürfen während Driftmessung nicht adjusten” ist richtig – aber mit einer Nuance

✅ **Richtig:**
Wenn du **ein festes Messfenster** verwendest (z.B. 20s), dann solltest du in diesem Fenster **keinen Step** machen (und idealerweise auch keine aggressiven Änderungen), sonst verfälschst du die Ableitung.

✅ **Noch besser / übliche Praxis:**
Statt “erst messen, dann korrigieren”, kannst du es **kontinuierlich** machen:

* jede Sekunde Offset messen
* `df = (offset[n] - offset[n-1]) / Δt`
* `df_ema = α*df + (1-α)*df_ema`
* `freq_adjust += df_ema`

Das ist mathematisch dasselbe wie dein “2-Momenten-Vergleich”, nur stabiler (ständig kleine Updates statt Sprung alle 20–60s). Genau so ist es in eurem Step-3-Plan formuliert. 

---

### Kompatibilität mit euren Dokus/Plänen

**Ja, kompatibel** – deine 3 Schritte entsprechen direkt:

* **Initial Offset Correction** = “Step correction” Phase (grober Abgleich)
* **Drift Baseline / Drift Evaluation** = “Frequency-error Servo” Idee (`dPhase/dt`)
* **Frequency Adjust** = `clock_adjtime(ADJ_FREQUENCY)`-Pfad und “cumulative_freq_ppb” Tracking

Und eure Architektur trennt das auch sauber:

* **PhcCalibrator** (20 PPS, drift vs PPS) für Initial-Frequenz-Estimate 
* **Servo Engines** (PI oder Frequency-Error) für den laufenden Betrieb 

---

### Was ich an deiner Beschreibung leicht “verschieben” würde (damit es 100% robust wird)

Damit es nicht wieder in “Step-Orgie” kippt:

✅ **Regel 1:** *Step nur bei “groß”* (z.B. >100ms oder >1s)
✅ **Regel 2:** Nach jedem Step: **Drift-Baseline reset + Samples skippen** (weil Discontinuity)
→ Das steht bei euch sinngemäß bereits als Fix-Denke drin. 
✅ **Regel 3:** Driftmessung immer auf **synchronem Offset**
Euer Plan hat dafür sogar explizit den Fix “SYNCHRONIZED_PHC_OFFSET_MEASUREMENT”, weil sonst “stale offsets” Steps auslösen können. 

---

### Mini-Checkliste: “dein Ansatz, aber production-tauglich”

Wenn ihr es so implementiert, ist es textbook-stabil:

* **Startup:**

  1. PPS-UTC Lock / Base mapping fixieren
  2. 1× Step PHC auf GPS (nur einmal)
  3. 20-PPS Calibration → initial `freq_ppb0`
* **Run:**

  * offset jede Sekunde
  * `df = Δoffset/Δt`
  * EMA filter (α≈0.05…0.2)
  * `freq_total = freq_ppb0 + df_ema + (optional kleiner PI-Rest)`
  * Step nur bei “wirklich groß” oder wenn Lock verloren

Das ist **genau der Weg**, den eure Doku als Migration/Step-3 beschreibt. 

---

Wenn du willst, kann ich dir aus *deinem* 3-Phasen-Text eine **konkrete State-Machine-Skizze** machen (inkl. “no adjust during measurement window”, “skip samples after step”, “when to reset baseline”), so dass es 1:1 als Implementations-Kommentar/ToDo in eurem Code passt.
